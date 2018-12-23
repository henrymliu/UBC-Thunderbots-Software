#include "mrf_backend.h"
#include <chrono>

MrfBackend::MrfBackend(MRFDongle& dongle) : dongle(dongle)
{

}

MrfBackend::~MrfBackend()
{
}

void MrfBackend::sendPrimitives(
        const std::vector<std::unique_ptr<Primitive>> &primitives)
{
	dongle.build_drive_packet(primitives);
}

void MrfBackend::update_detbots(std::vector<std::tuple<uint8_t, Point, Angle>> ft)
{
        detbots = ft;
}

void MrfBackend::update_ball(Ball b)
{
        ball = b;
}

void MrfBackend::send_vision_packet()
{
    dongle.send_camera_packet(detbots, ball.position(), 
        std::chrono::duration_cast<std::chrono::milliseconds>(
                ball.lastUpdateTimestamp().time_since_epoch()).count());
}