#pragma once

#include "../precompiled.h"

enum class MenuScreen
{
	Main,
	Pause
};

enum class MenuAction
{
	None,
	Play,
	Resume,
	Reset,
	Settings,
	Help,
	Exit
};

enum class Alignment
{
	LEFT,
	CENTER,
	RIGHT
};

struct Text
{
	float position_x = 0.0f;
	float position_y = 0.0f;
	std::string text;
	float scale = 1.0f;
	Alignment alignment = Alignment::LEFT;
	bool selected = false;
};

class Menu final
{
public:
	Menu(int width, int height);

	void Update(int width, int height);
	void Draw(bool hasStarted, bool paused);

	MenuAction ConsumeAction();

	const std::vector<Text>& GetTexts() const { return texts_; }
	std::vector<Text>& GetTextsMutable() { return texts_; }

	bool IsAnyModalOpen() const;

private:
	void BeginFrame();
	void DrawMain();
	void DrawPause();
	void DrawSettingsModal();
	void DrawHelpModal();

	void AddText(
		float u,
		float v,
		const std::string& text,
		float scale,
		Alignment alignment = Alignment::CENTER,
		bool selected = false
	);

	bool Button(
		float u,
		float v,
		const std::string& label,
		float scale,
		Alignment alignment,
		bool selected
	);

	float EstimateWidthPx(const std::string& s, float scale) const;
	float EstimateHeightPx(float scale) const;

	float Ui(float scale) const { return scale * ui_scale_; }

private:
	int width_ = 0;
	int height_ = 0;

	double mouse_x_ = 0.0;
	double mouse_y_ = 0.0;

	bool mouse_edge_down_ = false;

	int selected_ = 0;
	int last_up_state_ = GLFW_RELEASE;
	int last_down_state_ = GLFW_RELEASE;
	int last_enter_state_ = GLFW_RELEASE;
	int last_escape_state_ = GLFW_RELEASE;

	bool settings_open_ = false;
	bool help_open_ = false;

	float settings_anim_ = 0.0f;
	float help_anim_ = 0.0f;

	float ui_scale_ = 1.0f;
	bool show_controls_hint_ = true;
	bool shadows_enabled_ui_ = true;

	MenuAction pending_action_ = MenuAction::None;

	std::vector<Text> texts_;
};