#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include "referee.h"
#include "game/possession.h"
#include "entities/team.h"

/**
 * @brief Determines whether a goal has been scored.
 *
 * This function checks if the ball has completely crossed either goal line
 * while also being fully inside the vertical goal mouth.
 *
 * Important details:
 * - A goal is only valid if the *entire ball* (taking BALL_RADIUS into account)
 *   has crossed the goal line.
 * - The ball must be vertically contained between the goal posts.
 * - The right goal corresponds to Team 1 scoring.
 * - The left goal corresponds to Team 2 scoring.
 * @return
 * - 1 if Team 1 scores,
 * - 2 if Team 2 scores,
 * - 0 if no goal has occurred.
 */
static int goal(float x, float y) {
//y tirak bala , paiin
    float goal_top = CENTER_Y - (GOAL_HEIGHT / 2.0f);
    float goal_bottom = CENTER_Y + (GOAL_HEIGHT / 2.0f);
//khat darvaze ha
    float right_line = PITCH_X + PITCH_W;
    float left_line = PITCH_X;

    // check gool rast
    if (x - BALL_RADIUS > right_line) {
        if (y >= goal_top && y <= goal_bottom) {
            printf("GOAL! Right net hit at x:%.2f, y=%.2f\n", x, y);
            return 1;
        }
    }
    // check gool chap
    if (x + BALL_RADIUS < left_line) {
        if (y >= goal_top && y <= goal_bottom) {
            printf("GOAL! Left net hit at x:%.2f, y=%.2f\n", x, y);
            return 2;
        }
    }

    return 0; 
}

/**
 * @brief Checks whether the ball is out of bounds.
 *
 * The ball is considered out only when its *entire area* lies outside
 * the pitch boundaries. Partial overlap with the pitch does NOT count as out.
 *
 * Notes for students:
 * - Use BALL_RADIUS to ensure the whole ball has crossed a boundary.
 * - All four pitch sides (left, right, top, bottom) must be considered.
 * - This function does not handle goals; goal detection is performed separately.
 * @return true if the ball is fully out of bounds, false otherwise.
 */
static bool out(float x, float y) {
    float right_line = PITCH_X + PITCH_W;
    float left_line = PITCH_X;
    float bottom_line = PITCH_Y + PITCH_H;
    float top_line = PITCH_Y;

    if (x + BALL_RADIUS < left_line || x - BALL_RADIUS > right_line || 
        y + BALL_RADIUS < top_line || y - BALL_RADIUS > bottom_line) {
        
        printf("Ball is out: x=%.2f, y=%.2f\n", x, y); 
        return true;
    }
    
    return false; 
}

/**
 * @brief Acts as the game referee for one simulation step.
 *
 * This function is responsible for detecting and resolving game events
 * related to the ball, such as goals and out-of-bounds situations.
 *
 * Responsibilities:
 * - Check for goals BEFORE checking for out-of-bounds.
 * - Update team scores if a goal is detected.
 * - Report the appropriate game event.
 *
 * Notes for students:
 * - A goal must be checked first because a scored goal is technically out.
 * - If no event occurs, the game continues normally.
 *
 * @param scene Pointer to the current game scene.
 *
 * @return
 * - GOAL if a goal has been scored,
 * - OUT if the ball is out of bounds,
 * - 0 if no event occurred.
 */
int referee(struct Scene* scene) {

    float bx = scene->ball->position.x;
    float by = scene->ball->position.y;


    int goal_res = goal(bx, by);


    if (goal_res != 0) {
        if (goal_res == 1) {
            scene->first_team->score++; 
        } else {
            scene->second_team->score++;
        }
        return GOAL; 
    }

   
    if (out(bx, by)) {
        return OUT; 
    }

    return PLAY_ON;
}


/**
 * @brief Verifies the validity of a player's talent distribution.
 *
 * This function checks whether each individual talent value is within
 * the allowed range and whether the total talent points do not exceed
 * the maximum allowed per player.
 *
 * Notes for students:
 * - Each skill must be between 1 and MAX_TALENT_PER_SKILL (inclusive).
 * - The sum of all skills must not exceed MAX_TALENT_PER_PLAYER.
 * - Invalid configurations should be reported as errors.
 *
 * @param talents The talent structure to validate.
 */
void verify_talents(struct Talents talents) {
    int sum = talents.defence + talents.agility + talents.dribbling + talents.shooting;
    
    if (sum > MAX_TALENT_PER_PLAYER || 
        talents.defence > MAX_TALENT_PER_SKILL || talents.defence < 1 ||
        talents.agility > MAX_TALENT_PER_SKILL || talents.agility < 1 ||
        talents.dribbling > MAX_TALENT_PER_SKILL || talents.dribbling < 1 ||
        talents.shooting > MAX_TALENT_PER_SKILL || talents.shooting < 1) {
        
        printf("ERROR: Invalid talents! Values: defence=%d, agility=%d, dribbling=%d, shooting=%d, sum=%d\n",
               talents.defence, talents.agility, talents.dribbling, talents.shooting, sum);
    }
}


/**
 * @brief Verifies the correctness of a player's current state.
 *
 * Ensures that the player's state is consistent with the game rules.
 * In particular, only the player who currently possesses the ball
 * is allowed to be in the SHOOTING state.
 *
 * Notes for students:
 * - If a player attempts to shoot without possessing the ball,
 *   their state must be corrected.
 *
 * @param player Pointer to the player being verified.
 * @param scene  Pointer to the current game scene.
 */
void verify_state(struct Player *player, struct Scene *scene) {
    if (player->state == SHOOTING && scene->ball->possessor != player) {
        printf(" ERROR: the ball is not yours, you can't shoot! (team %d, player %d)\n",
                player->team, player->kit);
        
        player->state = MOVING;
    }
}

/**
 * @brief Verifies and limits a player's movement speed.
 *
 * This function ensures that a player's velocity does not exceed
 * the maximum allowed speed derived from their agility talent.
 *
 * Notes for students:
 * - Maximum speed scales linearly with the agility talent.
 * - Both x and y velocity components must be checked independently.
 * - If a component exceeds the limit, it must be clamped.
 *
 * @param player Pointer to the player whose movement is being verified.
 */
void verify_movement(struct Player *player) {
    float max_v = player->talents.agility * (MAX_PLAYER_VELOCITY / MAX_TALENT_PER_SKILL);

    
    if (player->velocity.x > max_v || player->velocity.x < -max_v) {
        printf(" ERROR: Demanding to run too fast in dimension x! (team %d, player %d)\n", player->team, player->kit);
        
        if (player->velocity.x > 0) player->velocity.x = max_v;
        else player->velocity.x = -max_v;
    }

    
    if (player->velocity.y > max_v || player->velocity.y < -max_v) {
        printf(" ERROR: Demanding to run too fast in dimension y! (team %d, player %d)\n", player->team, player->kit);
        
        if (player->velocity.y > 0) player->velocity.y = max_v;
        else player->velocity.y = -max_v;
    }
}

/**
 * @brief Verifies the validity of a ball shot.
 *
 * This function ensures that the ball's velocity after a shot does not
 * exceed the maximum allowed speed derived from the shooter's talent.
 *
 * Additional kickoff rules:
 * - During kickoff, the ball must be played into the player's own half.
 *
 * Notes for students:
 * - Ball velocity must be clamped if it exceeds the allowed maximum.
 * - Both velocity components must be checked independently.
 *
 * @param ball    Pointer to the ball being shot.
 * @param kickoff True if the shot occurs during kickoff.
 */
void verify_shoot(struct Ball *ball, bool kickoff) {
    float max_ball_v = MAX_BALL_VELOCITY;

    if (ball->velocity.x > max_ball_v || ball->velocity.x < -max_ball_v) {
        printf(" ERROR: Ball is moving too fast in dimension x!\n");
        if (ball->velocity.x > 0) ball->velocity.x = max_ball_v;
        else ball->velocity.x = -max_ball_v;
    }

    if (ball->velocity.y > max_ball_v || ball->velocity.y < -max_ball_v) {
        printf(" ERROR: Ball is moving too fast in dimension y!\n");
        if (ball->velocity.y > 0) ball->velocity.y = max_ball_v;
        else ball->velocity.y = -max_ball_v;
    }

    if (kickoff) {
        if ((ball->last_team == 1 && ball->velocity.x > 0) || 
            (ball->last_team == 2 && ball->velocity.x < 0)) {
            
            printf(" ERROR: You must kick the ball into your own half at the kick-off!\n");
            
            ball->velocity.x = 0;
            ball->velocity.y = 0;
        }
    }
}