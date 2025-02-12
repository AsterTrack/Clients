# NOTE: a vrpn tracker must call user callbacks with tracker data (pos and
#       ori info) which represent the transformation xfSourceFromSensor.
#       This means that the pos info is the position of the origin of
#       the sensor coord sys in the source coord sys space, and the
#       quat represents the orientation of the sensor relative to the
#       source space (ie, its value rotates the source's axes so that
#       they coincide with the sensor's)

import argparse
from datetime import datetime, timezone, timedelta

import sys
sys.path.append('lib/') # Path to vrpn.so
import vrpn

def vrpn_receiveTrackerPos(userdata, t):
	'''This function gets called when the tracker's POSITION xform is updated.
	userdata is whatever you passed into the register_change_handler function.
	VRPN passes it through to the callback handler. It's not used by vrpn internally.
	'''

	# To use timestamps, we need to explicitly convert to UTC
	timestamp = t['time'].replace(tzinfo=timezone.utc)
	latency = (datetime.now(timezone.utc) - timestamp) / timedelta(milliseconds=1)

	pos = t['position'] # x, y, z
	quat = t['quaternion'] # x, y, z, w

	print("vrpn_receiveTrackerPos\tSensor {:d} is at ({:.3f},{:.3f},{:.3f}) at time {} ({}ms ago)"
		.format(t['sensor'], pos[0], pos[1], pos[2], timestamp, latency)
	)
	print('  Callback userdata =', userdata)


def vrpn_receiveTrackerVel(userdata, t):
	'''This function gets called when the tracker's VELOCITY xform is updated.
	userdata is whatever you passed into the register_change_handler function.
	VRPN passes it through to the callback handler. It's not used by vrpn internally.
	'''

	# To use timestamps, we need to explicitly convert to UTC
	timestamp = t['time'].replace(tzinfo=timezone.utc)
	latency = (datetime.now(timezone.utc) - timestamp) / timedelta(milliseconds=1)

	vel = t['velocity'] # x, y, z
	vel_quat = t['future quaternion'] # x, y, z, w
	vel_quat_dt = t['future delta'] # dt

	print("vrpn_receiveTrackerVel\tSensor {:d} speed ({:.3f},{:.3f},{:.3f}) at time {} ({}ms ago)"
		.format(t['sensor'], vel[0], vel[1], vel[2], timestamp, latency)
	)
	print('  Callback userdata =', userdata)


if __name__ == '__main__':
	parser = argparse.ArgumentParser(
										prog = 'tracker_client',
										description = 'Connects to a tracker and prints position reports',
										epilog = 'Text at the bottom of help')
	parser.add_argument('device', help='Name of the device (like Tracker0@localhost)')
	args = parser.parse_args()

	done = False

	# Open the tracker and set its position callback handler
	# (other choices are velocity, acceleration, unit2ssensor, workspace,
	# tracker2room).  An optional fourth parameter selects a particular sensor
	# from the tracker to watch.
	tracker = vrpn.receiver.Tracker(args.device);
	tracker.register_change_handler(None, vrpn_receiveTrackerPos, "position");
	tracker.register_change_handler(None, vrpn_receiveTrackerVel, "velocity");

	while not done:
		# Let the traker do it's thing.
		# It will call the callback function(s) registered above when new
		# messages are received.
		tracker.mainloop()
