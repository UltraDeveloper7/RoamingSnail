#pragma once

#include "../precompiled.h"
#include "Player.hpp"
#include "../objects/Ball.hpp" 

class Ball; // fwd


class GameState {
public:
	static constexpr float SHOT_CLOCK_MAX = 60.0f;


	GameState();


	// Players / turns
	std::vector<Player>& Players() { return players_; }
	const std::vector<Player>& Players() const { return players_; }
	int CurrentPlayerIndex() const { return current_player_index_; }


	void ResetPlayersScores();
	void ResetPlayerIndex();


	// Shot/turn management
	void StartNewRack(); // resets flags for a rack (break pending)
	void StartNewTurn(); // called when a shot actually begins
	void SwitchTurn(bool cueBallDrawn); // advances player, sets BIH if cue not drawn


	// Shot clock & message ticker
	void ResetShotClock();
	bool TickShotClock(float dt); // returns true if expired *this frame*
	void TickMessage(float dt);


	// Transient shot flags (set by world on collisions)
	void MarkCueContact(int nonCueBallIndex);
	void ClearShotTransients();


	// Messages for HUD
	void SetMessage(const std::string& msg, float seconds);
	const std::string& Message() const { return current_message_; }
	float MessageTimeLeft() const { return message_timer_; }


	// Groups & fouls
	void AssignGroupFromBallNumber(int ballNumber);


	// Accessors/mutators used by rules
	bool IsGameOver() const { return is_game_over_; }
	void SetGameOver(bool v) { is_game_over_ = v; }


	bool IsFirstShot() const { return is_first_shot_; }
	void SetFirstShot(bool v) { is_first_shot_ = v; }


	bool IsAfterBreak() const { return is_after_break_; }
	void SetAfterBreak(bool v) { is_after_break_ = v; }

	// Mark that at least one ball hit a rail after legal contact in this shot
	void MarkRailContact();
	bool RailAfterContact() const;


	bool CheckRulesPending() const { return check_game_rules_; }
	void SetCheckRulesPending(bool v) { check_game_rules_ = v; }


	bool BallInHand() const { return ball_in_hand_; }
	void SetBallInHand(bool v) { ball_in_hand_ = v; }


	bool CueHitOtherBall() const { return cueBallHitOtherBall_; }
	void SetCueHitOtherBall(bool v) { cueBallHitOtherBall_ = v; }



	int FirstContactIndex() const { return firstBallContactIndex_; }


	int GroupOfPlayer(int playerIdx) const { return (playerIdx == 0) ? player1_ball_type_ : player2_ball_type_; }
	void SetGroupOfPlayer(int playerIdx, int group) { if (playerIdx == 0) player1_ball_type_ = group; else player2_ball_type_ = group; }


	int CurrentPlayerGroup() const { return GroupOfPlayer(current_player_index_); }


	float ShotClock() const { return shot_clock_; }
	void SetShotClock(float v) { shot_clock_ = v; }

	// Record a ball number the instant it’s pocketed on THIS shot.
	void NotePocketedThisShot(int ballNumber);

	// Read-only access for rules logic.
	const std::vector<int>& PocketedThisShot() const { return pocketed_this_shot_; }

	// Eight-ball initial position (for resetting after a foul)
	void SetEightInitialPos(const glm::vec3& p) { eight_initial_pos_ = p; }
	const glm::vec3& EightInitialPos() const { return eight_initial_pos_; }

private:
	std::vector<Player> players_{ Player("Player 1"), Player("Player 2") };
	int current_player_index_ = 0;


	bool is_game_over_ = false;
	bool is_first_shot_ = true; // table open at start
	bool is_after_break_ = true; // first resolution after break
	bool check_game_rules_ = false; // gate to run rules once when balls settle
	bool rail_after_contact_ = false;


	// per‑shot transients
	bool cueBallHitOtherBall_ = false;
	int firstBallContactIndex_ = -1; // -1: none; 8=8ball

	std::vector<int> pocketed_this_shot_; // ball numbers pocketed during this shot

	// groups: -1 unset, 0 solids, 1 stripes
	int player1_ball_type_ = -1;
	int player2_ball_type_ = -1;


	bool ball_in_hand_ = false;
	float shot_clock_ = SHOT_CLOCK_MAX;

	glm::vec3 eight_initial_pos_{ 0.0f, Ball::radius_, 0.0f };

	std::string current_message_;
	float message_timer_ = 0.0f;
};