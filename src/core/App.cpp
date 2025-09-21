#include "../precompiled.h"
#include "App.hpp"
#include "../objects/CueBallMap.hpp"
#include "../objects/Table.hpp"


// ------------------------------
// Post processing internals
// ------------------------------
namespace {
	GLuint sceneFBO = 0, sceneColor = 0, sceneDepth = 0;
	GLuint pingFBO[2]{ 0,0 }, pingColor[2]{ 0,0 };
	GLuint quadVAO = 0, quadVBO = 0;
	static GLuint guideVAO = 0, guideVBO = 0;
	static GLuint ringVAO = 0, ringVBO = 0;

	int    ppW = 0, ppH = 0;

	std::shared_ptr<Shader> blurShader, screenShader;
	static std::shared_ptr<Shader> lineShader;

	// Smooth 0→1 when menu opens, 1→0 when it closes
	static float g_menuFx = 0.0f;

	static GLuint makeColorTex(int w, int h) {
		GLuint t; glGenTextures(1, &t);
		glBindTexture(GL_TEXTURE_2D, t);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		return t;
	}

	static void createOrResizePost(int w, int h) {
		if (w <= 0 || h <= 0) return;
		const bool firstTime = (quadVAO == 0);

		// Fullscreen quad (once)
		if (firstTime) {
			float verts[] = {
				// pos      // uv
				-1.f,-1.f, 0.f,0.f,
				 1.f,-1.f, 1.f,0.f,
				 1.f, 1.f, 1.f,1.f,
				-1.f,-1.f, 0.f,0.f,
				 1.f, 1.f, 1.f,1.f,
				-1.f, 1.f, 0.f,1.f
			};
			glGenVertexArrays(1, &quadVAO);
			glGenBuffers(1, &quadVBO);
			glBindVertexArray(quadVAO);
			glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
			glEnableVertexAttribArray(1);
			glBindVertexArray(0);

			// Shaders 
			blurShader = std::make_shared<Shader>(Config::post_vertex_path,
				Config::blur_fragment_path);
			screenShader = std::make_shared<Shader>(Config::post_vertex_path,
				Config::screen_fragment_path);
		}

		// (Re)create scene FBO + depth
		if (sceneFBO == 0) glGenFramebuffers(1, &sceneFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);

		if (sceneColor) glDeleteTextures(1, &sceneColor);
		sceneColor = makeColorTex(w, h);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColor, 0);

		if (sceneDepth == 0) glGenRenderbuffers(1, &sceneDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, sceneDepth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneDepth);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// ping-pong FBOs
		if (pingFBO[0] == 0) glGenFramebuffers(2, pingFBO);
		if (pingColor[0]) glDeleteTextures(1, &pingColor[0]);
		if (pingColor[1]) glDeleteTextures(1, &pingColor[1]);
		pingColor[0] = makeColorTex(w, h);
		pingColor[1] = makeColorTex(w, h);
		for (int i = 0; i < 2; ++i) {
			glBindFramebuffer(GL_FRAMEBUFFER, pingFBO[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingColor[i], 0);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		ppW = w; ppH = h;
	}

	static void renderQuad() {
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	}

	static GLuint blurChain(GLuint inputTex, int passes) {
		if (!blurShader) return inputTex;
		blurShader->Bind();
		blurShader->SetInt(0, "src");
		blurShader->SetVec2(glm::vec2(1.f / ppW, 1.f / ppH), "texel");

		bool horizontal = true;
		GLuint cur = inputTex;
		for (int i = 0; i < passes; ++i) {
			glBindFramebuffer(GL_FRAMEBUFFER, pingFBO[horizontal ? 0 : 1]);
			glDisable(GL_DEPTH_TEST);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, cur);
			blurShader->SetVec2(horizontal ? glm::vec2(1, 0) : glm::vec2(0, 1), "dir");
			glClear(GL_COLOR_BUFFER_BIT);
			renderQuad();
			cur = pingColor[horizontal ? 0 : 1];
			horizontal = !horizontal;
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return cur;
	}

	static void present(GLuint tex, const glm::vec4& tint, float vignette,
		float aberration, float sharpen, float grain, float scanlines,
		float saturation, float contrast, float gamma, float timeSec)
	{
		if (!screenShader) return;
		screenShader->Bind();
		screenShader->SetInt(0, "src");
		screenShader->SetVec4(tint, "tint");
		screenShader->SetFloat(vignette, "vignette");

		// New optional effects (all neutral by default)
		screenShader->SetFloat(aberration, "aberration");
		screenShader->SetFloat(sharpen, "sharpen");
		screenShader->SetFloat(grain, "grain");
		screenShader->SetFloat(scanlines, "scanlines");
		screenShader->SetFloat(saturation, "saturation");
		screenShader->SetFloat(contrast, "contrast");
		screenShader->SetFloat(gamma, "gamma");
		screenShader->SetFloat(timeSec, "time");

		glDisable(GL_DEPTH_TEST);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		renderQuad();
	}

	static void ensureGuideResources() {
		if (guideVAO != 0 && ringVAO != 0 && lineShader) return;

		if (guideVAO == 0) {
			glGenVertexArrays(1, &guideVAO);
			glGenBuffers(1, &guideVBO);
			glBindVertexArray(guideVAO);
			glBindBuffer(GL_ARRAY_BUFFER, guideVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6, nullptr, GL_DYNAMIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glBindVertexArray(0);
		}

		if (ringVAO == 0) { // for the hollow impact circle
			glGenVertexArrays(1, &ringVAO);
			glGenBuffers(1, &ringVBO);
			glBindVertexArray(ringVAO);
			glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
			// room for 64 verts (x,y,z)
			glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * 64, nullptr, GL_DYNAMIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
			glBindVertexArray(0);
		}

		if (!lineShader)
			lineShader = std::make_shared<Shader>(Config::line_vertex_path, Config::line_fragment_path);
	}

	// solve |O + tD - C|^2 = R^2  (D must be normalized)
	static bool raySphere(const glm::vec3& O, const glm::vec3& D,
		const glm::vec3& C, float R, float& tHit)
	{
		const glm::vec3 oc = O - C;
		const float b = glm::dot(oc, D);
		const float c = glm::dot(oc, oc) - R * R;
		const float disc = b * b - c;
		if (disc < 0.0f) return false;
		const float t = -b - std::sqrt(disc);
		if (t <= 0.0f) return false;
		tHit = t;
		return true;
	}

	static void drawLine3D(const glm::mat4& view, const glm::mat4& proj,
		const glm::vec3& a, const glm::vec3& b,
		float width, const glm::vec3& color)
	{
		ensureGuideResources();
		glBindVertexArray(guideVAO);
		glBindBuffer(GL_ARRAY_BUFFER, guideVBO);
		float v[6] = { a.x,a.y,a.z, b.x,b.y,b.z };
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);

		lineShader->Bind();
		glm::mat4 mvp = proj * view;
		lineShader->SetMat4(mvp, "uMVP");
		lineShader->SetVec3(color, "uColor");
		glLineWidth(width);
		glDrawArrays(GL_LINES, 0, 2);
		glBindVertexArray(0);
	}

	// Hollow circle in XZ-plane (y fixed)
	static void drawCircleXZ(const glm::mat4& view, const glm::mat4& proj,
		const glm::vec3& center, float radius,
		int segments, float width, const glm::vec3& color)
	{
		ensureGuideResources();
		segments = std::min(std::max(segments, 12), 64);
		std::vector<float> verts; verts.reserve(3 * segments);
		for (int i = 0; i < segments; ++i) {
			float t = (float)i / (float)segments * 2.0f * glm::pi<float>();
			float cx = center.x + radius * std::cos(t);
			float cz = center.z + radius * std::sin(t);
			verts.push_back(cx);
			verts.push_back(center.y);
			verts.push_back(cz);
		}
		glBindVertexArray(ringVAO);
		glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * verts.size(), verts.data());

		lineShader->Bind();
		glm::mat4 mvp = proj * view;
		lineShader->SetMat4(mvp, "uMVP");
		lineShader->SetVec3(color, "uColor");
		glLineWidth(width);
		glDrawArrays(GL_LINE_LOOP, 0, segments);
		glBindVertexArray(0);
	}

	// Clip distance so that C + t*dir stays strictly inside cushions
	static float clipToTableXZ(const glm::vec3& C, const glm::vec3& dir,
		float tDesired, float margin)
	{
		float tMax = tDesired;

		if (dir.x > 0.0f)  tMax = std::min(tMax, (Table::bound_x_ - margin - C.x) / dir.x);
		if (dir.x < 0.0f)  tMax = std::min(tMax, (-Table::bound_x_ + margin - C.x) / dir.x);
		if (dir.z > 0.0f)  tMax = std::min(tMax, (Table::bound_z_ - margin - C.z) / dir.z);
		if (dir.z < 0.0f)  tMax = std::min(tMax, (-Table::bound_z_ + margin - C.z) / dir.z);

		return std::max(0.0f, tMax);
	}

	// Clip the ray O + t*dir against inner table bounds (XZ), keeping a margin
	static float clipRayToTableXZ(const glm::vec3& O, const glm::vec3& dir,
		float tDesired, float margin)
	{
		float tMax = tDesired;

		if (dir.x > 0.0f)  tMax = std::min(tMax, (Table::bound_x_ - margin - O.x) / dir.x);
		if (dir.x < 0.0f)  tMax = std::min(tMax, (-Table::bound_x_ + margin - O.x) / dir.x);
		if (dir.z > 0.0f)  tMax = std::min(tMax, (Table::bound_z_ - margin - O.z) / dir.z);
		if (dir.z < 0.0f)  tMax = std::min(tMax, (-Table::bound_z_ + margin - O.z) / dir.z);

		return std::max(0.0f, tMax);
	}

} // anonymous namespace

// -------------------------------------------------------------

App::App() :
	window_(std::make_unique<Window>()),
	text_renderer_(std::make_unique<TextRenderer>()),
	menu_(std::make_unique<Menu>(window_->GetWidth(), window_->GetHeight())),
	main_shader_(std::make_shared<Shader>(Config::vertex_path, Config::fragment_path)),
	background_shader_(std::make_shared<Shader>(Config::background_vertex_path, Config::background_fragment_path)),
	gui_shader_(std::make_shared<Shader>(Config::CueMap_vertex_path, Config::CueMap_fragment_path)),
	depthShader(std::make_shared<Shader>(Config::depth_vertex_path, Config::depth_fragment_path)),
	camera_(std::make_unique<Camera>()),
	cue_ball_map_(std::make_shared<CueBallMap>(*camera_, window_->GetGLFWWindow())),
	lightSpaceMatrices_{}
{
	//Logger::Init("log.txt");
	text_renderer_->Init();
	camera_->Init();

	// create post stack at current window size
	createOrResizePost(window_->GetWidth(), window_->GetHeight());
}

App::~App() {
	//Logger::Close();
}

void App::Run()
{
	while (!window_->ShouldClose())
	{
		glfwPollEvents();

		if (window_->Resized())
			OnResize();

		OnUpdate();

		glfwSwapBuffers(window_->GetGLFWWindow());
	}
}

void App::OnUpdate()
{
	const double current_frame = glfwGetTime();
	delta_time_ = current_frame - last_frame_;
	last_frame_ = current_frame;

	// Smooth step for menu animation (open/close)
	{
		const float target = in_menu_ ? 1.0f : 0.0f;
		const float speed = 6.0f; // larger = snappier
		g_menuFx += (target - g_menuFx) * (1.0f - std::exp(-speed * static_cast<float>(delta_time_)));
	}

	HandleState();

	// ---------- render scene into offscreen FBO ----------
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
	glViewport(0, 0, ppW, ppH);
	glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	if (world_)
	{
		camera_->UpdateViewMatrix(static_cast<float>(delta_time_));
		camera_->UpdateMain(main_shader_, *world_);

		environment_->Prepare();

		// 1) all shadow maps
		RenderShadowMap();

		// make sure we’re back rendering into the offscreen scene FBO
		glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
		glViewport(0, 0, ppW, ppH);

		// 2) bind depth maps & matrices in one go
		main_shader_->Bind();
		int total_lights = Config::light_count + (int)world_->GetLights().size();
		int finalLightCount = std::min(total_lights, Config::max_shader_lights);
		main_shader_->SetInt(finalLightCount, "lightCount");

		int units[Config::max_shader_lights];
		for (int i = 0; i < finalLightCount; ++i) {
			glActiveTexture(GL_TEXTURE0 + 9 + i);
			glBindTexture(GL_TEXTURE_2D, environment_->depthMap[i]);
			units[i] = 9 + i;
			std::string matName = "lightSpaceMatrix[" + std::to_string(i) + "]";
			main_shader_->SetMat4(lightSpaceMatrices_[i], matName.c_str());
		}
		main_shader_->SetIntArray("shadowMap[0]", units, finalLightCount);
		main_shader_->Unbind();

		world_->Update(static_cast<float>(delta_time_), !in_menu_);
		world_->Draw(main_shader_);

		camera_->UpdateBackground(background_shader_);
		environment_->Draw(background_shader_);

		// CueBallMap visibility
		bool isTopDownView = camera_->IsTopDownView();
		bool ballsMoving = world_->AreBallsInMotion();
		bool shouldVisible = isTopDownView && !ballsMoving;
		cue_ball_map_->SetVisible(shouldVisible);
		if (cue_ball_map_->IsVisible()) {
			cue_ball_map_->Draw();
			cue_ball_map_->HandleMouseInput(window_->GetGLFWWindow());
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0); // backbuffer

	// ---------- post: blur + tint, then draw text ----------
	const bool paused = in_menu_ && has_started_;
	const bool firstPage = in_menu_ && !has_started_;

	const float tSec = static_cast<float>(glfwGetTime());

	GLuint shown = sceneColor;
	if (paused) {
		shown = blurChain(sceneColor, 6); // stronger blur
		// Animated grading
		const float v = 0.15f * g_menuFx;
		const float ta = 0.18f * g_menuFx;                   // tint intensity
		const float sat = 1.0f - 0.15f * g_menuFx;            // slight desat
		const float con = 1.0f + 0.04f * g_menuFx;            // tiny contrast bump
		const float grn = 0.02f * g_menuFx;                   // very subtle grain

		present(shown,
			glm::vec4(0.0f, 0.0f, 0.0f, ta), v,
			/*aberration*/0.0f, /*sharpen*/0.0f, /*grain*/grn, /*scanlines*/0.0f,
			/*saturation*/sat, /*contrast*/con, /*gamma*/1.0f,
			tSec);
	}
	else if (firstPage) {
		shown = blurChain(sceneColor, 4); // mild blur
		const float v = 0.12f * g_menuFx;
		const float ta = 0.18f * g_menuFx;
		const float sat = 1.0f - 0.10f * g_menuFx;
		const float con = 1.0f + 0.03f * g_menuFx;
		const float grn = 0.015f * g_menuFx;

		present(shown,
			glm::vec4(0.055f, 0.415f, 0.239f, ta), v, // felt-green tint
			0.0f, 0.0f, grn, 0.0f,
			sat, con, 1.0f,
			tSec);
	}
	else {
		// Gameplay: fully neutral
		present(sceneColor,
			glm::vec4(0, 0, 0, 0), 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f,
			1.0f, 1.0f, 1.0f,
			tSec);
	}

	// ---- aiming guideline overlay (optional) ----
	if (world_ && !in_menu_ && menu_->IsGuidelineOn() && !world_->AreBallsInMotion())
	{
		const auto& balls = world_->GetBalls();
		const glm::vec3 O = balls[0]->translation_;
		const glm::vec3 D = glm::normalize(world_->GetCue()->AimDir());  // cue-ball travel dir

		// Find first object-ball hit (Minkowski radius = 2R)
		const float R = 2.0f * Ball::radius_;
		float bestT = std::numeric_limits<float>::max();
		int hitIdx = -1;
		for (int i = 1; i < (int)balls.size(); ++i) {
			if (!balls[i]->IsDrawn()) continue;
			float t;
			if (raySphere(O, D, balls[i]->translation_, R, t)) {
				if (t < bestT) { bestT = t; hitIdx = i; }
			}
		}

		const glm::mat4 view = camera_->GetViewMatrix();
		const glm::mat4 proj = camera_->GetProjectionMatrix();

		glDisable(GL_DEPTH_TEST); // HUD-style overlay

		// Clip the white line so it never crosses cushions
		const float margin = 0.025f; // keep a small distance from cushions
		float tBound = clipRayToTableXZ(O, D, 1000.0f, margin); // "far" but clipped to table
		float tLine = (hitIdx != -1) ? std::min(bestT, tBound) : tBound;

		glm::vec3 impact = O + D * tLine;
		drawLine3D(view, proj, O, impact, 2.0f, glm::vec3(1.0f));

		// Larger hollow circle at contact (only if a real hit and still inside bounds)
		if (hitIdx != -1 && tLine > 1e-4f) {
			const float ringR = Ball::radius_ * 0.60f;   // enlarged ring
			const float eps = Ball::radius_ * 0.03f;   // pull back slightly to avoid z-fighting
			glm::vec3 ringCenter = impact - D * eps;
			ringCenter.y = Ball::radius_;
			drawCircleXZ(view, proj, ringCenter, ringR, 64, 3.0f, glm::vec3(1.0f)); // more segments, thicker line
		}

		// Predicted object-ball direction; color depends on legality (still clipped to table)
		if (hitIdx != -1) {
			const bool legalTarget = world_->IsLegalAimTarget(hitIdx);

			const glm::vec3 C = balls[hitIdx]->translation_;
			const glm::vec3 objDir = glm::normalize(C - impact);   // line-of-centers
			const float want = 0.90f;                            // desired preview length
			const float margin2 = 0.025f;                          // same cushion distance
			float tClipped = clipToTableXZ(C, objDir, want, margin2);
			if (tClipped > 1e-4f) {
				glm::vec3 endY = C + objDir * tClipped;
				glm::vec3 col = legalTarget ? glm::vec3(1.0f, 1.0f, 0.0f)
					: glm::vec3(1.0f, 0.1f, 0.1f);
				drawLine3D(view, proj, C, endY, 2.0f, col);
			}
		}
	}

	// ---- text UI pass ----
	GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
	glDisable(GL_DEPTH_TEST);

	if (world_) {
		const auto& players = world_->GetPlayers();
		const auto& current_player = players[world_->GetCurrentPlayerIndex()];
		int curIdx = world_->GetCurrentPlayerIndex();
		const std::string& shownName = players[curIdx].GetName(); // use synced, real player name
		menu_->AddText(0.0f, 0.95f, "Current Player: " + shownName, 0.6f);


		float clockSec = world_->GetShotClock();
		menu_->AddText(0.0f, 0.90f, "Shot Clock: " + std::to_string((int)clockSec), 0.6f);

		const std::string msg = world_->GetMessage();
		if (!msg.empty())
			menu_->AddText(0.35f, 0.95f, msg, 0.75f);
	}

	if (in_menu_)
		menu_->Draw(world_ == nullptr, has_started_);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	text_renderer_->Render(menu_->GetTexts());
	glDisable(GL_BLEND);

	// ------------------------------
	// Handle menu clicks (missing before)
	// ------------------------------
	if (in_menu_) {
		// Play / Resume
		if (menu_->ConsumePlayClicked()) {
			in_menu_ = false;
			glfwSetInputMode(window_->GetGLFWWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (!world_) {
				Load();
				has_started_ = true;            // mark game as started
				world_->ResetPlayerIndex();
				world_->UpdatePlayerNames(menu_->P1Name(), menu_->P2Name());
				last_frame_ = glfwGetTime();
			}
		}
		// Reset
		if (menu_->ConsumeResetClicked() && world_) {
			world_->Reset();
			world_->ResetGame();
			world_->UpdatePlayerNames(menu_->P1Name(), menu_->P2Name()); // update names on reset
		}
		// Exit
		if (menu_->ConsumeExitClicked()) {
			window_->SetCloseFlag();
		}
	}

	// Push any newly edited names into the actual players
	if (world_) {
		std::string n1, n2;
		if (menu_->ConsumeEditedNames(n1, n2)) {
			world_->UpdatePlayerNames(n1, n2);
		}
	}

	if (depthWasEnabled) glEnable(GL_DEPTH_TEST);

	// light toggles 0..9
	static bool key_was_pressed[10] = { false };
	GLFWwindow* w = window_->GetGLFWWindow();
	for (int key = GLFW_KEY_0; key <= GLFW_KEY_9; ++key) {
		int lightIndex = key - GLFW_KEY_0;
		bool isPressed = (glfwGetKey(w, key) == GLFW_PRESS);
		if (isPressed && !key_was_pressed[lightIndex]) {
			if (world_) world_->ToggleLight(lightIndex);
			key_was_pressed[lightIndex] = true;
		}
		else if (!isPressed && key_was_pressed[lightIndex]) {
			key_was_pressed[lightIndex] = false;
		}
	}
}

void App::OnResize() const
{
	const int new_width = window_->GetWidth(), new_height = window_->GetHeight();

	glViewport(0, 0, new_width, new_height);

	if (camera_)
		camera_->UpdateProjectionMatrix(new_width, new_height);

	menu_->Update(new_width, new_height);

	text_renderer_->UpdateProjectionMatrix(new_width, new_height);
	text_renderer_->Update();

	// Update CueBallMap window size
	if (cue_ball_map_)
		cue_ball_map_->UpdateWindowSize();

	// resize post stack
	createOrResizePost(new_width, new_height);

	window_->ResetResizedFlag();
}

void App::Load()
{
	world_ = std::make_unique<World>(cue_ball_map_, *camera_);
	environment_ = std::make_unique<Environment>();
	menu_->InstallCharCallback(window_->GetGLFWWindow());

	camera_->Init();
	world_->Init();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	glEnable(GL_MULTISAMPLE);
	glViewport(0, 0, window_->GetWidth(), window_->GetHeight());

	main_shader_->Bind();
	main_shader_->SetInt(1, "irradianceMap");
	main_shader_->SetInt(2, "prefilterMap");
	main_shader_->SetInt(3, "brdfLUT");
	main_shader_->SetInt(4, "material.diffuseMap");
	main_shader_->SetInt(5, "material.roughnessMap");
	main_shader_->SetInt(6, "material.normalMap");
	main_shader_->SetInt(7, "material.aoMap");
	main_shader_->SetInt(8, "material.metallicMap");
	main_shader_->Unbind();
}

void App::RenderShadowMap()
{
	// --- save current framebuffer & viewport (so we can restore them) ---
	GLint prevFBO = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
	GLint vp[4];       glGetIntegerv(GL_VIEWPORT, vp);

	// We'll produce an orthographic projection for each shadow:
	const float S = Config::shadow_extent;
	glm::mat4 lightProjection = glm::ortho(-S, S, -S, S, Config::near_plane, Config::far_plane);

	const auto& physicalLights = world_->GetLights();
	const int total_lights = Config::light_count + static_cast<int>(physicalLights.size());
	const int finalLightCount = std::min(total_lights, Config::max_shader_lights);

	for (int i = 0; i < finalLightCount; ++i)
	{
		glm::vec3 lightPos;
		bool isOn = true;

		if (i < Config::light_count) {
			const float light_position_x = (i % 2) ? 2.0f * i : -2.0f * i;
			lightPos = glm::vec3(light_position_x, 2.0f, 0.0f);
		}
		else {
			const int physicalIndex = i - Config::light_count;
			if (physicalIndex >= static_cast<int>(physicalLights.size())) break;
			auto& l = physicalLights[physicalIndex];
			lightPos = l->GetPosition();
			isOn = l->IsOn();
		}
		if (!isOn) continue;

		glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0, 1, 0));
		glm::mat4 lightSpaceMatrix = lightProjection * lightView;
		lightSpaceMatrices_[i] = lightSpaceMatrix;

		depthShader->Bind();
		depthShader->SetMat4(lightSpaceMatrix, "lightSpaceMatrix");

		glViewport(0, 0, Config::shadow_width, Config::shadow_height);
		glBindFramebuffer(GL_FRAMEBUFFER, environment_->depthMapFBO[i]);
		glClear(GL_DEPTH_BUFFER_BIT);

		world_->Draw(depthShader);
	}

	// --- restore framebuffer & viewport exactly as they were ---
	glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
	glViewport(vp[0], vp[1], vp[2], vp[3]);
}


void App::HandleState()
{
	GLFWwindow* window = window_->GetGLFWWindow();

	// --- Edge-trigger for ESC ---
	static int prevEsc = GLFW_RELEASE;
	const int escNow = glfwGetKey(window, GLFW_KEY_ESCAPE);
	const bool escPressed = (escNow == GLFW_PRESS && prevEsc == GLFW_RELEASE);

	// Ask the menu whether any modal is open (settings/help)
	bool modalOpen = false;
	if (menu_) {
		modalOpen = menu_->IsSettingsOpen() || menu_->IsHelpOpen();
	}

	// 1) In-game -> ESC opens the menu
	if (!in_menu_) {
		if (escPressed) {
			in_menu_ = true;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			prevEsc = escNow;
			return;
		}
	}
	// 2) In menu and game NOT started and NO modal -> ESC quits
	else {
		if (!has_started_ && escPressed && !modalOpen) {
			window_->SetCloseFlag();
			prevEsc = escNow;
			return;
		}
		// Note: when a modal is open, ESC is handled inside Menu.cpp to close it.
	}

	// 3) Only handle camera toggle when NOT in menu
	if (!in_menu_) {
		static int last_mouse_button_state = GLFW_RELEASE;
		const int mouse_button_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

		if (mouse_button_state == GLFW_PRESS && last_mouse_button_state == GLFW_RELEASE) {
			const bool cueMapVisible = (cue_ball_map_ && cue_ball_map_->IsVisible());
			const bool cueMapHit = (cue_ball_map_ && cue_ball_map_->IsWithinBounds());

			if (!cueMapVisible || !cueMapHit) {
				const bool newTopDown = !camera_->IsTopDownView();
				camera_->SetTopDownView(newTopDown);

				glfwSetInputMode(
					window,
					GLFW_CURSOR,
					newTopDown ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED
				);
			}
		}

		last_mouse_button_state = mouse_button_state;
	}

	prevEsc = escNow;
}
