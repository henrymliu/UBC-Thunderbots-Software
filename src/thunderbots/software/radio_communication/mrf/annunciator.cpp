
#include "annunciator.h"
#include "dongle.h"
#include "util/logger/init.h"
#include "constants.h"
#include "shared/constants.h"
#include "util/codec.h"

namespace
{
    /**
     * \brief The number of attempts to request the build IDs before giving up.
     */
    const unsigned int REQUEST_BUILD_IDS_COUNT = 7;

    /**
     * \brief The number of seconds to wait between consecutive requests for
     * the build IDs.
     */
    const double REQUEST_BUILD_IDS_INTERVAL = 0.5;

    struct RSSITableEntry final
    {
        int rssi;
        int db;
    };
    const struct RSSITableEntry RSSI_TABLE[] = {
        {255, -35}, {254, -36}, {253, -37}, {250, -38}, {245, -39}, {239, -40},
        {233, -41}, {228, -42}, {225, -43}, {221, -44}, {216, -45}, {212, -46},
        {207, -47}, {203, -48}, {198, -49}, {193, -50}, {188, -51}, {183, -52},
        {176, -53}, {170, -54}, {165, -55}, {159, -56}, {153, -57}, {148, -58},
        {143, -59}, {138, -60}, {133, -61}, {129, -62}, {125, -63}, {121, -64},
        {117, -65}, {111, -66}, {107, -67}, {100, -68}, {95, -69},  {89, -70},
        {83, -71},  {78, -72},  {73, -73},  {68, -74},  {63, -75},  {58, -76},
        {53, -77},  {48, -78},  {43, -79},  {37, -80},  {32, -81},  {27, -82},
        {23, -83},  {18, -84},  {13, -85},  {9, -86},   {5, -87},   {2, -88},
        {1, -89},   {0, -90},
    };

}  // namespace

Annunciator::Annunciator(ros::NodeHandle& node_handle)
{
    robot_status_publisher = node_handle.advertise<thunderbots_msgs::MRFMessages>(
        Util::Constants::ROBOT_STATUS_TOPIC, 1);
    mrf_message.robot_statuses.resize(MAX_ROBOTS_OVER_RADIO);
}

thunderbots_msgs::MRFMessages Annunciator::handle_robot_message(int index, const void *data,
                                                         std::size_t len, uint8_t lqi,
                                                         uint8_t rssi)
{
    thunderbots_msgs::RobotStatus robot_status;

    robot_status.link_quality = lqi / 255.0;

    {
        bool found = false;
        for (std::size_t i = 0; !found && i < sizeof(RSSI_TABLE) / sizeof(*RSSI_TABLE);
             ++i)
        {
            if (RSSI_TABLE[i].rssi < rssi)
            {
                robot_status.received_signal_strength_db = RSSI_TABLE[i].db;
                found                                    = true;
            }
        }
        if (!found)
        {
            robot_status.received_signal_strength_db = -90;
        }
    }

    const uint8_t *bptr = static_cast<const uint8_t *>(data);
    if (len)
    {
        switch (*bptr)
        {
            case 0x00:
                // General robot status update
                ++bptr;
                --len;
                if (len >= 13)
                {
                    robot_status.robot = index;
                    robot_status.alive = true;

                    robot_status.battery_voltage =
                        (bptr[0] | static_cast<unsigned int>(bptr[1] << 8)) / 1000.0;
                    bptr += 2;
                    len -= 2;

                    robot_status.capacitor_voltage =
                        (bptr[0] | static_cast<unsigned int>(bptr[1] << 8)) / 100.0;
                    // TODO: let visualizer know about low cap voltages?
                    // low_capacitor_message.active(capacitor_voltage < 5.0);
                    bptr += 2;
                    len -= 2;

                    robot_status.break_beam_reading =
                        (bptr[0] | static_cast<unsigned int>(bptr[1] << 8)) / 1000.0;
                    bptr += 2;
                    len -= 2;

                    robot_status.board_temperature =
                        (bptr[0] | static_cast<unsigned int>(bptr[1] << 8)) / 100.0;
                    bptr += 2;
                    len -= 2;

                    robot_status.ball_in_beam      = !!(*bptr & 0x80);
                    robot_status.capacitor_charged = !!(*bptr & 0x40);
                    // unsigned int logger_status = *bptr & 0x3F;
                    // for (std::size_t i = 0; i < logger_messages.size(); ++i)
                    // {
                    //     if (logger_messages[i])
                    //     {
                    //         logger_messages[i]->active(logger_status == i);
                    //     }
                    // }
                    ++bptr;
                    --len;

                    for (std::size_t i = 0; i < MRF::SD_MESSAGES.size(); ++i)
                    {
                        if (MRF::SD_MESSAGES[i] && (*bptr == i))
                        {
                            robot_status.messages.push_back(MRF::SD_MESSAGES[i]);
                        }
                    }
                    ++bptr;
                    --len;

                    robot_status.dribbler_speed =
                        static_cast<int16_t>(
                            static_cast<uint16_t>(bptr[0] | (bptr[1] << 8))) *
                        25 * 60 / 6;
                    bptr += 2;
                    len -= 2;

                    robot_status.dribbler_temperature = *bptr++;
                    --len;

                    bool has_error_extension = false;
                    while (len)
                    {
                        // Decode extensions.
                        switch (*bptr)
                        {
                            case 0x00:  // Error bits.
                                ++bptr;
                                --len;
                                if (len >= MRF::ERROR_BYTES)
                                {
                                    has_error_extension = true;
                                    for (unsigned int i = 0; i < MRF::ERROR_LT_COUNT;
                                         ++i)
                                    {
                                        unsigned int byte = i / CHAR_BIT;
                                        unsigned int bit  = i % CHAR_BIT;

                                        if (bptr[byte] & (1 << bit) && MRF::ERROR_LT_MESSAGES[i])
                                        {
                                            robot_status.messages.push_back(MRF::ERROR_LT_MESSAGES[i]);
                                        }
                                    }
                                    for (unsigned int i = 0; i < MRF::ERROR_ET_COUNT;
                                         ++i)
                                    {
                                        unsigned int byte =
                                            (i + MRF::ERROR_LT_COUNT) / CHAR_BIT;
                                        unsigned int bit =
                                            (i + MRF::ERROR_LT_COUNT) % CHAR_BIT;
                                        // TODO: Handle these messages
                                        if (bptr[byte] & (1 << bit) && MRF::ERROR_ET_MESSAGES[i])
                                        {
                                            robot_status.messages.push_back(MRF::ERROR_ET_MESSAGES[i]);
                                        }
                                    }
                                    bptr += MRF::ERROR_BYTES;
                                    len -= MRF::ERROR_BYTES;
                                }
                                else
                                {
                                    LOG(WARNING) << "Received general robot status "
                                                    "update with truncated error bits "
                                                    "extension of length "
                                                 << len << std::endl;
                                }
                                break;

                            case 0x01:  // Build IDs.
                                ++bptr;
                                --len;
                                if (len >= 8)
                                {
                                    robot_status.build_ids_valid = true;
                                    robot_status.fw_build_id     = decode_u32_le(bptr);
                                    robot_status.fpga_build_id = decode_u32_le(bptr + 4);
                                    // TODO: check this
                                    // check_build_id_mismatch();
                                    bptr += 8;
                                    len -= 8;
                                }
                                else
                                {
                                    LOG(WARNING) << "Received general robot status "
                                                    "update with truncated build IDs "
                                                    "extension of length "
                                                 << len << std::endl;
                                }
                                break;

                            case 0x02:  // LPS data.
                                ++bptr;
                                --len;
                                if (len >= 4)
                                {
                                    // for (unsigned int i = 0; i < 4; ++i)
                                    // {
                                    //     lps_values[i] =
                                    //         static_cast<int8_t>(*bptr++) / 10.0;
                                    // }
                                    len -= 4;
                                }
                                else
                                {
                                    LOG(WARNING) << "Received general robot status "
                                                    "update with truncated LPS data "
                                                    "extension of length "
                                                 << len << std::endl;
                                }
                                break;

                            default:
                                LOG(WARNING)
                                    << "Received general status packet from "
                                       "robot with unknown extension code "
                                    << static_cast<unsigned int>(*bptr) << std::endl;
                                len = 0;
                                break;
                        }
                    }

                    if (!has_error_extension)
                    {
                        // Error reporting extension is absent → no errors are
                        // asserted.
                        // TODO: figure this out
                        // for (auto &i : error_lt_messages)
                        // {
                        //     i->active(false);
                        // }
                    }
                }
                else
                {
                    LOG(WARNING) << "Received general robot status update with wrong "
                                    "byte count "
                                 << len << std::endl;
                }

                // TODO: see what this does
                // if (!robot_status.build_ids_valid &&
                //     request_build_ids_timer.elapsed() >
                //         REQUEST_BUILD_IDS_INTERVAL)
                // {
                //     request_build_ids_timer.stop();
                //     request_build_ids_timer.reset();
                //     request_build_ids_timer.start();
                //     if (request_build_ids_counter)
                //     {
                //         --request_build_ids_counter;
                //         static const uint8_t REQUEST = 0x0D;
                //         dongle_.send_unreliable(index, 20, &REQUEST, 1);
                //     }
                //     else
                //     {
                //         build_id_fetch_error_message.active(true);
                //     }
                // }
                break;

            case 0x01:
                // Autokick fired
                // TODO handle this somehow
                // signal_autokick_fired.emit();
                break;

            case 0x04:
                // Robot has ball
                robot_status.ball_in_beam = true;
                break;

            case 0x05:
                // Robot does not have ball
                robot_status.ball_in_beam = false;
                break;

            default:
                LOG(WARNING) << "Received packet from robot with unknown message type "
                             << static_cast<unsigned int>(*bptr) << std::endl;
                break;
        }
    }

    mrf_message.robot_statuses[index] = robot_status;
    robot_status_publisher.publish(mrf_message);
    return mrf_message;
}

thunderbots_msgs::MRFMessages Annunciator::handle_status(uint8_t status) 
{
    mrf_message.dongle_messages.clear();

    if (static_cast<MRFDongle::EStopState>(status & 3U) == MRFDongle::EStopState::BROKEN) 
    {
        mrf_message.dongle_messages.push_back(MRF::ESTOP_BROKEN_MESSAGE);
    }

    if (status & 4U)
    {
        mrf_message.dongle_messages.push_back(MRF::RX_FCS_FAIL_MESSAGE);
    }
    if (status & 8U)
    {
        mrf_message.dongle_messages.push_back(MRF::SECOND_DONGLE_MESSAGE);
    }
    if (status & 16U)
    {
        mrf_message.dongle_messages.push_back(MRF::TRANSMIT_QUEUE_FULL_MESSAGE);
    }
    if (status & 32U)
    {
        mrf_message.dongle_messages.push_back(MRF::RECEIVE_QUEUE_FULL_MESSAGE);
    }

    robot_status_publisher.publish(mrf_message);
    return mrf_message;
}
