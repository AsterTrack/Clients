/**
AsterTrack Optical Tracking System
Copyright (C)  2025 Seneral <contact@seneral.dev> and contributors

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "vrpn.hpp"

#include "vrpn/quat.h"

#include "util/log.hpp"


/*
 * VRPN (Virtual Reality Private Network) interface to output tracking data to other programs (locally or on the network)
 */


/* Integrating VRPN into a more modern C++ style */

template<> void OpaqueDeleter<vrpn_Connection>::operator()(vrpn_Connection* ptr) const
{ ptr->removeReference(); }


/* Conversion from internally used steady_clock to timeval used by VRPN */

static inline struct timeval createTimestamp(TimePoint_t time)
{
	// Create timestamp without assuming steady_clock to be equivalent to timeval
	struct timeval time_now, timestamp;
	vrpn_gettimeofday(&time_now, NULL);
	TimePoint_t time_ref = sclock::now();
	long diffUS = std::chrono::duration_cast<std::chrono::microseconds>(time_ref - time).count();
	long long timeUS = time_now.tv_sec * 1000000 + time_now.tv_usec;
	timeUS -= diffUS;
	timestamp.tv_sec = timeUS/1000000;
	timestamp.tv_usec = timeUS%1000000;
	LOG(LIO, LTrace, "Time %.2fms ago resulted in timestamp (%ld, %ld) from cur time (%ld, %ld) and diff (%ldus)",
		std::chrono::duration_cast<std::chrono::microseconds>(time_ref - time).count()/1000.0f,
		timestamp.tv_sec, timestamp.tv_usec, time_now.tv_sec, time_now.tv_usec, diffUS)
	return timestamp;
}

static inline TimePoint_t getTimestamp(struct timeval time_msg)
{
	// Recreate timestamp without assuming steady_clock to be equivalent to timeval
	struct timeval time_now;
	vrpn_gettimeofday(&time_now, NULL);
	TimePoint_t timestamp = sclock::now();
	timestamp -= std::chrono::seconds(time_now.tv_sec-time_msg.tv_sec);
	timestamp -= std::chrono::microseconds(time_now.tv_usec-time_msg.tv_usec);
	return timestamp;
}


/* Wrapper for a VRPN remote tracker */

static void handleTrackerPosRot(void *data, const vrpn_TRACKERCB t)
{
	vrpn_Tracker_Wrapper *tracker = (vrpn_Tracker_Wrapper*)data;
	tracker->lastTimestamp = getTimestamp(t.msg_time);
	tracker->lastPacket = sclock::now();
	tracker->receivedPackets = true;

	if (tracker->logPackets)
	{
		LOG(LIO, LInfo, "Tracker %s, sensor %d pose update!", tracker->path.c_str(), t.sensor);
		LOG(LIO, LInfo, "Pos: (%f, %f, %f)", t.pos[0], t.pos[1], t.pos[2]);
		LOG(LIO, LInfo, "Rot: (%f, %f, %f, %f)", t.quat[0], t.quat[1], t.quat[2], t.quat[3]);
	}

	Eigen::Vector3d pos = Eigen::Vector3d(t.pos[0], t.pos[1], t.pos[2]);
	Eigen::Matrix3d rot = Eigen::Quaterniond(t.quat[Q_W], t.quat[Q_X], t.quat[Q_Y], t.quat[Q_Z]).toRotationMatrix();
	Eigen::Isometry3f pose;
	pose.linear() = rot.cast<float>();
	pose.translation() = pos.cast<float>();
	tracker->pose = pose;
}

static void handleTrackerVelocity(void *data, const vrpn_TRACKERVELCB t)
{ // Currently not sent by AsterTrack
	vrpn_Tracker_Wrapper *tracker = (vrpn_Tracker_Wrapper*)data;
	tracker->lastTimestamp = getTimestamp(t.msg_time);
	tracker->lastPacket = sclock::now();
	tracker->receivedPackets = true;

	if (tracker->logPackets)
	{
		LOG(LIO, LInfo, "Tracker %s, sensor %d velocity update!", tracker->path.c_str(), t.sensor);
		LOG(LIO, LInfo, "dT: (%f, %f, %f)", t.vel[0], t.vel[1], t.vel[2]);
		LOG(LIO, LInfo, "dR: (%f, %f, %f, %f)", t.vel_quat[0], t.vel_quat[1], t.vel_quat[2], t.vel_quat[3]);
	}
}

static void handleTrackerAccel(void *data, const vrpn_TRACKERACCCB t)
{ // Currently not sent by AsterTrack
	vrpn_Tracker_Wrapper *tracker = (vrpn_Tracker_Wrapper*)data;
	tracker->lastTimestamp = getTimestamp(t.msg_time);
	tracker->lastPacket = sclock::now();
	tracker->receivedPackets = true;

	if (tracker->logPackets)
	{
		LOG(LIO, LInfo, "Tracker %s, sensor %d accel update!", tracker->path.c_str(), t.sensor);
		LOG(LIO, LInfo, "ddT: (%f, %f, %f)", t.acc[0], t.acc[1], t.acc[2]);
		LOG(LIO, LInfo, "ddR: (%f, %f, %f, %f)", t.acc_quat[0], t.acc_quat[1], t.acc_quat[2], t.acc_quat[3]);
	}
}

vrpn_Tracker_Wrapper::vrpn_Tracker_Wrapper(std::string Path)
{
	path = std::move(Path);
	if (path.find('@') == path.npos)
		path += "@localhost";
}

void vrpn_Tracker_Wrapper::connect()
{
	remote = std::make_unique<vrpn_Tracker_Remote>(path.c_str());
	remote->register_change_handler(this, handleTrackerPosRot);
	remote->register_change_handler(this, handleTrackerVelocity);
	remote->register_change_handler(this, handleTrackerAccel);
}

bool vrpn_Tracker_Wrapper::isConnected()
{
	if (!remote) return false;
	struct timeval now;
	struct timeval diff;
	vrpn_gettimeofday(&now, NULL);
	diff = vrpn_TimevalDiff(now, remote->time_last_ping_response);
	vrpn_TimevalNormalize(diff);
	return diff.tv_sec < 3;
}

std::vector<std::string> vrpn_getKnownTrackers(vrpn_Connection *connection)
{
	std::vector<std::string> trackers;

	// Sadly, this uses d_dispatcher, which does not differentiate between local and remote sender name
	// d_senders of an endpoint in d_endpoints might have more differentiating info (e.g. assigned remote id)
	// but these are all protected members
	int i = 1;
	while (true)
	{
		const char *senderName = connection->sender_name(i++);
		if (!senderName) break;
		trackers.emplace_back(senderName);
	}

	return trackers;
}
