#include "../precompiled.h"
#include "GameRules.hpp"
#include "GameState.hpp"
#include "../objects/Ball.hpp"


// 0 = solids, 1 = stripes, -1 = neither (cue=0, eight=8)
static int BallTypeFromNumber(int num) {
    if (num >= 1 && num <= 7)  return 0;
    if (num >= 9 && num <= 15) return 1;
    return -1;
}



bool GameRules::AreAllGroupBallsPocketed(const std::vector<std::shared_ptr<Ball>>& balls, int groupType)
{
    if (groupType == -1) return false;
    for (size_t k = 1; k < balls.size(); ++k) {
        if (!balls[k]->IsDrawn()) continue;                 // already pocketed -> ignore
        int num = balls[k]->GetNumber();
        if (num == 0 || num == 8) continue;                 // ignore cue / eight
        if (BallTypeFromNumber(num) == groupType) return false; // still on table
    }
    return true;
}


static void RespotEightBall(const std::vector<std::shared_ptr<Ball>>& balls,
    const glm::vec3& spot)
{
    for (auto& b : balls) {
        if (b->GetNumber() == 8) {
            b->PlaceAt(spot);
            break;
        }
    }
}

void GameRules::EvaluateEndOfShot(const std::vector<std::shared_ptr<Ball>>& balls, GameState& s)
{
    if (s.IsGameOver()) return;

    bool foul = false;
    bool ballPocketed = false;          // for this shot only
    bool eightBallPocketed = false;
    bool pottedSolid = false;           // this shot only
    bool pottedStripe = false;          // this shot only
    bool pottedOwn = false;             // this shot only (meaningful when groups assigned)

    bool nonEightPocketed = false;

    const bool onBreak = s.IsAfterBreak();
    const bool tableOpen = s.IsFirstShot();
    const int  curGroup = s.CurrentPlayerGroup(); // -1 if not assigned

    // If both player groups are already set, table is no longer open.
    if (s.GroupOfPlayer(0) != -1 && s.GroupOfPlayer(1) != -1)
        s.SetFirstShot(false);

    // 1) cue ball pocketed => foul & BIH
    if (!balls[0]->IsDrawn()) {
        foul = true;
        s.SetBallInHand(true);
    }

    // 2) no contact => foul (unless currently placing BIH)
    if (!s.CueHitOtherBall() && !s.BallInHand()) {
        foul = true;
        s.SetBallInHand(true);
    }

    // 3) first-contact rule
    if (!foul) {
        const int firstIdx = s.FirstContactIndex();
        if (firstIdx != -1) {
            const int firstNum = balls[firstIdx]->GetNumber();

            if (tableOpen && !onBreak) {
                if (firstNum == 8) { foul = true; s.SetBallInHand(true); }
            }
            else if (!tableOpen && curGroup != -1) {
                if (firstNum == 8 && !AreAllGroupBallsPocketed(balls, curGroup)) {
                    foul = true; s.SetBallInHand(true);
                }
                else if (firstNum != 8) {
                    const int contactType = BallTypeFromNumber(firstNum);
                    if (contactType != curGroup) { foul = true; s.SetBallInHand(true); }
                }
            }
        }
    }

    // 4) analyze only the balls pocketed *this shot*
    for (int num : s.PocketedThisShot()) {
        ballPocketed = true;

        if (num == 8) {
            eightBallPocketed = true;
            continue;
        }

		nonEightPocketed = true;
        const int type = BallTypeFromNumber(num);
        if (type == 0) pottedSolid = true;
        if (type == 1) pottedStripe = true;

        if (!tableOpen && curGroup != -1 && type == curGroup) {
            pottedOwn = true;
            s.Players()[s.CurrentPlayerIndex()].AddScore(1);
        }
    }

    // 5) cushion-after-contact: if no pocket this shot, at least one ball must hit a rail
    if (!ballPocketed && !s.RailAfterContact()) {
        foul = true;
        s.SetBallInHand(true);
    }

    // 6) 8-ball outcomes
    if (eightBallPocketed && onBreak) {
        RespotEightBall(balls, s.EightInitialPos());

        if (foul) {
            s.SetMessage("Scratch on the break while 8 fell — 8 respotted, BIH to opponent.", 1.6f);
            s.SwitchTurn(balls[0]->IsDrawn());
        }
        else {
            // Breaker κρατά μόνο αν μπήκε ΚΑΙ κάποια άλλη μπάλα εκτός από το 8
            if (nonEightPocketed) {
                s.ResetShotClock();
                s.SetMessage("8-ball on the break — respotted. Breaker continues.", 1.2f);
            }
            else {
                s.SetMessage("8-ball on the break — respotted. Dry break.", 1.2f);
                s.SwitchTurn(balls[0]->IsDrawn());
            }
        }
        return;
    }

    if (eightBallPocketed) {
        const bool cleared = AreAllGroupBallsPocketed(balls, curGroup);
        if (cleared && !foul) {
            s.SetGameOver(true);
            s.SetMessage("8-ball pocketed — WIN!", 2.f);
            return;
        }

        // Παράνομο 8-ball: respot + φάουλ + BIH
        RespotEightBall(balls, s.EightInitialPos());
        foul = true;
        s.SetBallInHand(true);
        s.SetMessage("8-ball pocketed illegally — respotted. Ball in hand for opponent.", 1.6f);
        // δεν κάνουμε return: αφήνουμε το γενικό foul resolution (#8) να αλλάξει σειρά κ.λπ.
    }

    // 7) post-break (no 8-ball)
    if (onBreak) {
        s.SetAfterBreak(false);

        if (foul) {
            s.SetMessage("Foul on the break. Ball in hand for opponent.", 1.2f);
            s.SwitchTurn(balls[0]->IsDrawn());
        }
        else {
            if (ballPocketed) {
                s.ResetShotClock();      // breaker keeps table; assignment later
            }
            else {
                s.SwitchTurn(balls[0]->IsDrawn()); // dry break
            }
        }
        return;
    }

    // 8) normal turn resolution (not break)
    if (foul) {
        s.SetMessage("Foul! Ball in hand for opponent.", 1.2f);
        s.SwitchTurn(balls[0]->IsDrawn());
    }
    else {
        bool shooterKeeps = false;

        if (curGroup == -1) {
            // Table open (post-break): any non-8 pocket this shot keeps the table
            shooterKeeps = (pottedSolid || pottedStripe);
        }
        else {
            // Groups assigned: must pocket OWN color this shot to keep the table
            shooterKeeps = pottedOwn;
        }

        if (shooterKeeps) s.ResetShotClock();
        else              s.SwitchTurn(balls[0]->IsDrawn()); // legal hit, no make → pass turn
    }

    // 9) group assignment: open table, not the break, exactly one color fell this shot
    if (!foul && tableOpen && !onBreak) {
        int assignType = -1;
        if (pottedSolid ^ pottedStripe) assignType = pottedSolid ? 0 : 1; // exactly one color
        if (assignType != -1) s.AssignGroupFromBallNumber(assignType == 0 ? 1 : 9);
        // both colors fell -> remain open; shooterKeeps handled above
    }
}
