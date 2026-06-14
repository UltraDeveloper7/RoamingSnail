#include "../precompiled.h"
#include "Menu.hpp"

namespace
{
	static bool g_anyHoverThisFrame = false;
	static GLFWcursor* g_cursorHand = nullptr;
	static GLFWcursor* g_cursorArrow = nullptr;

	static float EaseOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return 1.0f - std::pow(1.0f - t, 3.0f);
	}

	static float Towards(float cur, float target, float dt, float tau)
	{
		const float k = 1.0f - std::exp(-dt / tau);
		float out = cur + (target - cur) * k;
		if (std::fabs(out - target) < 0.006f)
		{
			out = target;
		}
		return out;
	}
}

Menu::Menu(int width, int height)
	: width_(width)
	, height_(height)
{
}

void Menu::Update(int width, int height)
{
	width_ = width;
	height_ = height;
}

MenuAction Menu::ConsumeAction()
{
	MenuAction action = pending_action_;
	pending_action_ = MenuAction::None;
	return action;
}

bool Menu::IsAnyModalOpen() const
{
	return settings_open_ || help_open_ || settings_anim_ > 0.001f || help_anim_ > 0.001f;
}

void Menu::BeginFrame()
{
	texts_.clear();

	GLFWwindow* window = glfwGetCurrentContext();

	double mx = 0.0;
	double my = 0.0;
	glfwGetCursorPos(window, &mx, &my);

	mouse_x_ = mx;
	mouse_y_ = height_ - my;

	static int prevMouse = GLFW_RELEASE;
	const int curMouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	mouse_edge_down_ = curMouse == GLFW_PRESS && prevMouse == GLFW_RELEASE;
	prevMouse = curMouse;

	g_anyHoverThisFrame = false;
}

float Menu::EstimateWidthPx(const std::string& s, float scale) const
{
	constexpr float avgGlyphWidth = 0.62f;
	return static_cast<float>(s.size())
		* static_cast<float>(Config::default_font_size)
		* avgGlyphWidth
		* scale;
}

float Menu::EstimateHeightPx(float scale) const
{
	return static_cast<float>(Config::default_font_size) * 1.1f * scale;
}

void Menu::AddText(
	float u,
	float v,
	const std::string& text,
	float scale,
	Alignment alignment,
	bool selected
)
{
	texts_.push_back(Text{
		u * static_cast<float>(width_),
		v * static_cast<float>(height_),
		text,
		scale,
		alignment,
		selected
		});
}

bool Menu::Button(
	float u,
	float v,
	const std::string& label,
	float scale,
	Alignment alignment,
	bool selected
)
{
	const float px = u * static_cast<float>(width_);
	const float py = v * static_cast<float>(height_);

	const float w = EstimateWidthPx(label, scale);
	const float h = EstimateHeightPx(scale);

	float x0 = px;
	float x1 = px + w;

	if (alignment == Alignment::CENTER)
	{
		x0 = px - w * 0.5f;
		x1 = px + w * 0.5f;
	}
	else if (alignment == Alignment::RIGHT)
	{
		x0 = px - w;
		x1 = px;
	}

	const float y0 = py - h * 0.5f;
	const float y1 = py + h * 0.5f;

	const bool hover =
		mouse_x_ >= x0 && mouse_x_ <= x1 &&
		mouse_y_ >= y0 && mouse_y_ <= y1;

	if (hover)
	{
		g_anyHoverThisFrame = true;
	}

	const float drawScale = hover || selected ? scale * 1.06f : scale;
	AddText(u, v, label, drawScale, alignment, hover || selected);

	return hover && mouse_edge_down_;
}

void Menu::DrawMain()
{
	AddText(0.5f, 0.82f, "Roaming Snail", Ui(1.65f), Alignment::CENTER, true);
	AddText(0.5f, 0.74f, "A small journey through dry rolling hills", Ui(0.55f), Alignment::CENTER, false);

	if (Button(0.5f, 0.56f, "Play", Ui(0.95f), Alignment::CENTER, selected_ == 0))
	{
		pending_action_ = MenuAction::Play;
	}

	if (Button(0.5f, 0.48f, "Settings", Ui(0.80f), Alignment::CENTER, selected_ == 1))
	{
		settings_open_ = true;
	}

	if (Button(0.5f, 0.41f, "How to Play", Ui(0.80f), Alignment::CENTER, selected_ == 2))
	{
		help_open_ = true;
	}

	if (Button(0.5f, 0.34f, "Exit", Ui(0.80f), Alignment::CENTER, selected_ == 3))
	{
		pending_action_ = MenuAction::Exit;
	}
}

void Menu::DrawPause()
{
	AddText(0.5f, 0.80f, "Paused", Ui(1.45f), Alignment::CENTER, true);

	if (Button(0.5f, 0.62f, "Resume", Ui(0.92f), Alignment::CENTER, selected_ == 0))
	{
		pending_action_ = MenuAction::Resume;
	}

	if (Button(0.5f, 0.54f, "Reset Snail", Ui(0.78f), Alignment::CENTER, selected_ == 1))
	{
		pending_action_ = MenuAction::Reset;
	}

	if (Button(0.5f, 0.47f, "Settings", Ui(0.78f), Alignment::CENTER, selected_ == 2))
	{
		settings_open_ = true;
	}

	if (Button(0.5f, 0.40f, "How to Play", Ui(0.78f), Alignment::CENTER, selected_ == 3))
	{
		help_open_ = true;
	}

	if (Button(0.5f, 0.33f, "Exit", Ui(0.78f), Alignment::CENTER, selected_ == 4))
	{
		pending_action_ = MenuAction::Exit;
	}
}

void Menu::DrawSettingsModal()
{
	const float a = EaseOutCubic(settings_anim_);
	const float scale = 0.85f + 0.15f * a;

	AddText(0.5f, 0.68f, "Settings", Ui(1.10f) * scale, Alignment::CENTER, true);

	const std::string uiScaleText = std::format("UI scale: {:.0f}%", ui_scale_ * 100.0f);
	AddText(0.5f, 0.58f, uiScaleText, Ui(0.72f) * scale, Alignment::CENTER, false);

	if (Button(0.42f, 0.51f, "<", Ui(0.85f) * scale, Alignment::CENTER, false))
	{
		ui_scale_ = std::max(0.65f, ui_scale_ - 0.05f);
	}

	if (Button(0.58f, 0.51f, ">", Ui(0.85f) * scale, Alignment::CENTER, false))
	{
		ui_scale_ = std::min(1.25f, ui_scale_ + 0.05f);
	}

	const std::string controlsText = std::string(show_controls_hint_ ? "[x] " : "[ ] ") + "Show controls hint";
	if (Button(0.5f, 0.43f, controlsText, Ui(0.68f) * scale, Alignment::CENTER, false))
	{
		show_controls_hint_ = !show_controls_hint_;
	}

	const std::string shadowText = std::string(shadows_enabled_ui_ ? "[x] " : "[ ] ") + "Soft shadows";
	if (Button(0.5f, 0.36f, shadowText, Ui(0.68f) * scale, Alignment::CENTER, false))
	{
		shadows_enabled_ui_ = !shadows_enabled_ui_;
	}

	if (Button(0.5f, 0.26f, "Close [Esc]", Ui(0.72f) * scale, Alignment::CENTER, false))
	{
		settings_open_ = false;
	}
}

void Menu::DrawHelpModal()
{
	const float a = EaseOutCubic(help_anim_);
	const float scale = 0.85f + 0.15f * a;

	AddText(0.5f, 0.73f, "How to Play", Ui(1.05f) * scale, Alignment::CENTER, true);

	AddText(0.5f, 0.62f, "Move snail: Arrow keys", Ui(0.66f) * scale, Alignment::CENTER, false);
	AddText(0.5f, 0.56f, "Retract / roll shell: R", Ui(0.66f) * scale, Alignment::CENTER, false);
	AddText(0.5f, 0.50f, "Eat dry bushes for a speed boost", Ui(0.66f) * scale, Alignment::CENTER, false);
	AddText(0.5f, 0.44f, "Shell mode slows down when hitting vegetation", Ui(0.66f) * scale, Alignment::CENTER, false);
	AddText(0.5f, 0.38f, "Camera: W A S D, Q/E, mouse", Ui(0.66f) * scale, Alignment::CENTER, false);
	AddText(0.5f, 0.32f, "Lock / unlock mouse: L", Ui(0.66f) * scale, Alignment::CENTER, false);

	if (Button(0.5f, 0.22f, "Close [Esc]", Ui(0.72f) * scale, Alignment::CENTER, false))
	{
		help_open_ = false;
	}
}

void Menu::Draw(bool hasStarted, bool paused)
{
	BeginFrame();

	GLFWwindow* window = glfwGetCurrentContext();

	static double prevTime = 0.0;
	const double now = glfwGetTime();

	if (prevTime == 0.0)
	{
		prevTime = now;
	}

	const float dt = static_cast<float>(now - prevTime);
	prevTime = now;

	settings_anim_ = Towards(settings_anim_, settings_open_ ? 1.0f : 0.0f, dt, 0.10f);
	help_anim_ = Towards(help_anim_, help_open_ ? 1.0f : 0.0f, dt, 0.10f);

	const bool modalOpen = IsAnyModalOpen();

	const int up = glfwGetKey(window, GLFW_KEY_UP);
	const int down = glfwGetKey(window, GLFW_KEY_DOWN);
	const int enter = glfwGetKey(window, GLFW_KEY_ENTER);
	const int enter2 = glfwGetKey(window, GLFW_KEY_KP_ENTER);
	const int esc = glfwGetKey(window, GLFW_KEY_ESCAPE);

	const bool upEdge = up == GLFW_PRESS && last_up_state_ == GLFW_RELEASE;
	const bool downEdge = down == GLFW_PRESS && last_down_state_ == GLFW_RELEASE;
	const bool enterEdge =
		(enter == GLFW_PRESS || enter2 == GLFW_PRESS) &&
		last_enter_state_ == GLFW_RELEASE;
	const bool escEdge = esc == GLFW_PRESS && last_escape_state_ == GLFW_RELEASE;

	if (escEdge)
	{
		if (settings_open_)
		{
			settings_open_ = false;
		}
		else if (help_open_)
		{
			help_open_ = false;
		}
	}

	if (!modalOpen)
	{
		const int maxIndex = hasStarted && paused ? 4 : 3;

		if (upEdge)
		{
			selected_ = std::max(0, selected_ - 1);
		}

		if (downEdge)
		{
			selected_ = std::min(maxIndex, selected_ + 1);
		}

		if (enterEdge)
		{
			if (!hasStarted)
			{
				if (selected_ == 0) pending_action_ = MenuAction::Play;
				if (selected_ == 1) settings_open_ = true;
				if (selected_ == 2) help_open_ = true;
				if (selected_ == 3) pending_action_ = MenuAction::Exit;
			}
			else if (paused)
			{
				if (selected_ == 0) pending_action_ = MenuAction::Resume;
				if (selected_ == 1) pending_action_ = MenuAction::Reset;
				if (selected_ == 2) settings_open_ = true;
				if (selected_ == 3) help_open_ = true;
				if (selected_ == 4) pending_action_ = MenuAction::Exit;
			}
		}
	}

	const bool modalVisible =
		settings_anim_ > 0.001f ||
		help_anim_ > 0.001f ||
		settings_open_ ||
		help_open_;

	if (!modalVisible)
	{
		if (!hasStarted)
		{
			DrawMain();
		}
		else if (paused)
		{
			DrawPause();
		}
		else if (show_controls_hint_)
		{
			AddText(0.02f, 0.96f, "Arrows: move  |  R: shell  |  L: mouse lock", Ui(0.45f), Alignment::LEFT, false);
		}
	}

	if (settings_anim_ > 0.001f)
	{
		DrawSettingsModal();
	}

	if (help_anim_ > 0.001f)
	{
		DrawHelpModal();
	}

	last_up_state_ = up;
	last_down_state_ = down;
	last_enter_state_ = (enter == GLFW_PRESS || enter2 == GLFW_PRESS) ? GLFW_PRESS : GLFW_RELEASE;
	last_escape_state_ = esc;

	if (GLFWwindow* cw = glfwGetCurrentContext())
	{
		if (!g_cursorHand)
		{
			g_cursorHand = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
		}

		if (!g_cursorArrow)
		{
			g_cursorArrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		}

		glfwSetCursor(cw, g_anyHoverThisFrame ? g_cursorHand : g_cursorArrow);
	}
}