/**
 * Implementation of the PasserTactic
 */
#include "software/ai/hl/stp/tactic/passer_tactic.h"

#include <g3log/g3log.hpp>

#include "shared/constants.h"
#include "software/ai/hl/stp/action/kick_action.h"
#include "software/ai/hl/stp/action/move_action.h"
#include "software/ai/hl/stp/tactic/tactic_visitor.h"
#include "software/geom/util.h"

using namespace Passing;

PasserTactic::PasserTactic(Passing::Pass pass, const Ball& ball, bool loop_forever)
    : Tactic(loop_forever, {RobotCapabilities::Capability::Kick}),
      pass(std::move(pass)),
      ball(ball)
{
}

std::string PasserTactic::getName() const
{
    return "Passer Tactic";
}

void PasserTactic::updateWorldParams(const Ball& updated_ball)
{
    this->ball = updated_ball;
}

void PasserTactic::updateControlParams(const Pass& updated_pass)
{
    this->pass = updated_pass;
}

double PasserTactic::calculateRobotCost(const Robot& robot, const World& world)
{
    // Prefer robots closer to the pass start position
    // We normalize with the total field length so that robots that are within the field
    // have a cost less than 1
    double cost =
        (robot.position() - pass.passerPoint()).length() / world.field().totalXLength();
    return std::clamp<double>(cost, 0, 1);
}

void PasserTactic::calculateNextAction(ActionCoroutine::push_type& yield)
{
    auto move_action = std::make_shared<MoveAction>(
        MoveAction::ROBOT_CLOSE_TO_DEST_THRESHOLD, Angle(), true);
    // Move to a position just behind the ball (in the direction of the pass)
    // until it's time to perform the pass
    while (ball.lastUpdateTimestamp() < pass.startTime())
    {
        // We want to wait just behind where the pass is supposed to start, so that the
        // ball is *almost* touching the kicker
        Vector ball_offset =
            Vector::createFromAngle(pass.passerOrientation())
                .normalize(DIST_TO_FRONT_OF_ROBOT_METERS + BALL_MAX_RADIUS_METERS * 2);
        Point wait_position = pass.passerPoint() - ball_offset;

        move_action->updateControlParams(*robot, wait_position, pass.passerOrientation(),
                                         0, DribblerEnable::OFF, MoveType::NORMAL,
                                         AutokickType::NONE, BallCollisionType::ALLOW);
        yield(move_action);
    }

    // The angle between the ball velocity vector and a vector from the passer
    // point to the receiver point
    Angle ball_velocity_to_pass_orientation;

    auto kick_action = std::make_shared<KickAction>();
    do
    {
        // We want the robot to move to the starting position for the shot and also
        // rotate to the correct orientation to face the shot
        kick_action->updateWorldParams(ball);
        kick_action->updateControlParams(*robot, ball.position(), pass.receiverPoint(),
                                         pass.speed());
        yield(kick_action);

        // We want to keep trying to kick until the ball is moving along the pass
        // vector with sufficient velocity
        Angle passer_to_receiver_angle =
            (pass.receiverPoint() - pass.passerPoint()).orientation();
        ball_velocity_to_pass_orientation =
            ball.velocity().orientation().minDiff(passer_to_receiver_angle);
    } while (ball_velocity_to_pass_orientation.abs() > Angle::fromDegrees(20) ||
             ball.velocity().length() < 0.5);
}

void PasserTactic::accept(TacticVisitor& visitor) const
{
    visitor.visit(*this);
}
