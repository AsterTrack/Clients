bl_info = {
	"name": "VRPN Streamer",
	"description": "Streams VRPN tracking data",
	"author": "Seneral",
	"version": (1, 0),
	"blender": (4, 3, 0),
	"location": "View3D > UI > VRPN",
	"category": "Animation",
}

import bpy, bpy_extras
import math
from mathutils import Matrix, Euler, Vector, Quaternion
import threading
import time
from datetime import datetime, timezone, timedelta
from collections import deque, defaultdict
import array
import bisect

import sys

# VRPN Library Path
vrpnLibPath = 'lib'
if vrpnLibPath not in sys.path:
	sys.path.append(vrpnLibPath)
import vrpn

class VRPNTrackerInfo:
	def __init__(self):
		self.trackerSetup = None
		self.trackerVRPN = None
		self.trackingQueue = deque()
		self.hasReceived = False
		self.hasRecorded = False
		self.firstReceiveTime = None
		self.firstRecordTime = None

vrpn_lock = threading.Lock()
vrpn_thread = None
vrpn_streaming = False
vrpn_trackers = dict()

recording_startFrame = None
recording_startTime = None
recording_database = None

class RecordedTracker:
	def __init__(self, frame, mat):
		self.frame = frame
		self.mat = mat

def vrpn_CommsThread():
	global vrpn_lock
	global vrpn_thread
	global vrpn_streaming
	global vrpn_trackers

	while vrpn_streaming:
		with vrpn_lock:
			for name, t in vrpn_trackers.items():
				t.trackerVRPN.mainloop()
		time.sleep(0.001)

def vrpn_startStreaming(context):
	global vrpn_lock
	global vrpn_thread
	global vrpn_streaming
	global vrpn_trackers

	settings = context.scene.VRPNSettings

	# Update vrpn_trackers with trackers setup in UI
	with vrpn_lock:
		for tracker in settings.trackers:
			if not tracker.vrpnPath: continue
			if tracker.vrpnPath in vrpn_trackers: continue
			trk = VRPNTrackerInfo()
			trk.trackerSetup = tracker
			path = tracker.vrpnPath
			if not "@" in path:
				path = path + "@localhost"
			trk.trackerVRPN = vrpn.receiver.Tracker(path)
			trk.trackerVRPN.register_change_handler(trk, vrpn_receiveTrackerPos, "position")
			trk.trackerVRPN.register_change_handler(trk, vrpn_receiveTrackerVel, "velocity")
			vrpn_trackers[tracker.vrpnPath] = trk
		# Start thread if not already running
		vrpn_streaming = True
		if vrpn_thread is None:
			vrpn_thread = threading.Thread(target=vrpn_CommsThread)
			vrpn_thread.start()

def vrpn_stopStreaming(context):
	global vrpn_lock
	global vrpn_thread
	global vrpn_streaming
	global vrpn_trackers

	vrpn_streaming = False
	if vrpn_thread is not None:
		vrpn_thread.join()
	with vrpn_lock:
		vrpn_thread = None
		for name, trk in vrpn_trackers.items():
			del trk.trackerVRPN
		vrpn_trackers.clear()

def vrpn_receiveTrackerPos(trk, t):
	trk.trackingQueue.append(t)
	if not trk.hasReceived:
		print("First pose for tracker '{}' received at {}!".format(trk.trackerSetup.vrpnPath, t['time']))
		trk.firstReceiveTime = t['time']
		trk.hasReceived = True

def vrpn_receiveTrackerVel(trk, t):
	pass

def vrpn_parsePacket(t):
	time = t['time'].replace(tzinfo=timezone.utc);
	pos = Vector((t['position'][0], t['position'][1], t['position'][2]))
	quat = Quaternion((t['quaternion'][3], t['quaternion'][0], t['quaternion'][1], t['quaternion'][2]))
	mat = Matrix.LocRotScale(pos, quat, None)
	return time, mat

def mapRecordingTime(time):
	# Find frame/time for accurate recording
	recTime = (time-recording_startTime)/timedelta(seconds=1)
	recFrame = recTime*bpy.context.scene.render.fps + recording_startFrame
	return recTime, recFrame

def getTrackerTarget(tracker):
	if not tracker.target: return None
	target = bpy.data.objects[tracker.target]

	if target.type == 'ARMATURE' and tracker.use_subtarget:
		if not tracker.subtarget: return None
		target = target.pose.bones[tracker.subtarget]

	return target

def getTargetTransform(tracker, target, mat):
	if tracker.origin:
		origin = bpy.data.objects[tracker.origin]
		mat = origin.matrix_world @ mat
	if tracker.use_local_offset:
		m_local_pos = Matrix.Translation(tracker.local_pos)
		m_local_rot = Euler((math.radians(tracker.local_rot[0]),math.radians(tracker.local_rot[1]),math.radians(tracker.local_rot[2]))).to_matrix().to_4x4()
		m_local_scale = Matrix.Scale(tracker.local_scale[0],4,(1,0,0)) @ Matrix.Scale(tracker.local_scale[1],4,(0,1,0)) @ Matrix.Scale(tracker.local_scale[2],4,(0,0,1))
		mat = mat @ m_local_pos @ m_local_rot @ m_local_scale
	if target.bl_rna.name == 'Pose Bone':
		mat = target.id_data.convert_space(pose_bone=target, matrix=mat, from_space='WORLD', to_space='LOCAL')
	return mat

def applyTargetTransform(target, mat):
	if target.bl_rna.name == 'Pose Bone':
		target.matrix = mat
	else:
		target.matrix_world = mat

def getTargetFCurves(target):
	if not target.animation_data:
		target.animation_data_create()
	if not target.animation_data.action:
		target.animation_data.action = bpy.data.actions.new(f"{target.name}_MOCAP")
	action = target.animation_data.action
	# Determine FCurves to fetch
	posID = 'location'
	pos = [ None ] * 3
	if target.rotation_mode == 'QUATERNION':
		rotID = 'rotation_quaternion'
		rot = [ None ] * 4
	else:
		rotID = 'rotation_euler'
		rot = [ None ] * 3
	# Find/Init FCurves
	for i in range(0,len(pos)):
		pos[i] = action.fcurve_ensure_for_datablock(datablock = target, data_path = posID, index=i)
	for i in range(0,len(rot)):
		rot[i] = action.fcurve_ensure_for_datablock(datablock = target, data_path = rotID, index=i)

	return pos, rot
		

def updateTrackerStates(context):
	global recording_startFrame
	global recording_startTime
	global recording_database

	settings = context.scene.VRPNSettings

	applyPose = settings.apply_pose_directly
	keyframing = settings.record_keyframes or (context.screen.is_animation_playing and context.tool_settings.use_keyframe_insert_auto)
	recording = settings.record_internally or keyframing
	if not recording:
		recording_startFrame = None
		recording_startTime = None
	elif recording_startFrame is None:
		recording_startFrame = context.scene.frame_current
		recording_startTime = datetime.now(timezone.utc)

	if settings.record_internally and not recording_database:
		recording_database = defaultdict(list)

	newData = False
	with vrpn_lock:
		for tracker in settings.trackers:
			if not tracker.vrpnPath in vrpn_trackers: continue
			trk = vrpn_trackers[tracker.vrpnPath]
			if settings.record_internally:
				db = recording_database[tracker.vrpnPath]

			target = getTrackerTarget(tracker)
			""" if target and keyframing:
				target.keyframe_delete('location')
				target.keyframe_delete('rotation_quaternion')
				target.keyframe_delete('rotation_euler')
				target.keyframe_delete('scale') """
			if target and keyframing:
				posFC, rotFC = getTargetFCurves(target)
				old_rot = target.rotation_quaternion.copy() if target.rotation_mode == 'QUATERNION' else target.rotation_euler.copy()

			mat = None
			while trk.trackingQueue:
				t = trk.trackingQueue.popleft()
				newData = True

				time, mat = vrpn_parsePacket(t)

				if not recording: continue

				recTime, recFrame = mapRecordingTime(time)

				if settings.record_internally:

					db.append(RecordedTracker(recFrame, mat))

					if not trk.hasRecorded:
						print("    First pose for tracker '{}' recorded at {}!".format(trk.trackerSetup.vrpnPath, time))
						trk.firstRecordTime = time
						trk.hasRecorded = True

				if target and keyframing and recTime >= 0:

					# Insert keyframe by applying and recording
					applyTargetTransform(target, getTargetTransform(tracker, target, mat))
					target.keyframe_insert('location', frame=recFrame)
					if target.rotation_mode == 'QUATERNION':
						target.keyframe_insert('rotation_quaternion', frame=recFrame)
					else:
						target.keyframe_insert('rotation_euler', frame=recFrame)

					# Insert keyframe directly
					""" tgt_mat = getTargetTransform(tracker, target, mat)
					pos = tgt_mat.to_translation()
					for i in range(0,len(posFC)):
						posFC[i].keyframe_points.insert(recFrame, pos[i], options={'FAST'})
					if target.rotation_mode == 'QUATERNION':
						rot = tgt_mat.to_quaternion()
						rot.make_compatible(old_rot)
					else:
						rot = tgt_mat.to_euler(target.rotation_euler.order, old_rot)
					for i in range(0,len(rotFC)):
						rotFC[i].keyframe_points.insert(recFrame, rot[i], options={'FAST'}) """

				if not trk.hasRecorded:
					print("    First pose for tracker '{}' recorded at {}!".format(trk.trackerSetup.vrpnPath, time))
					trk.firstRecordTime = time
					trk.hasRecorded = True

			# Too slow still
			""" if target and keyframing:
				for i in range(0,len(posFC)):
					posFC[i].update()
				for i in range(0,len(rotFC)):
					rotFC[i].update() """

			if settings.apply_pose_directly and target and mat:
				applyTargetTransform(target, getTargetTransform(tracker, target, mat))

	return newData

def updateTrackerStatesTimer():
	if not vrpn_streaming:
		return None

	updateTrackerStates(bpy.context)

	return 1/60

class ToggleStreaming(bpy.types.Operator):
	bl_idname = "vrpn.toggle_streaming"
	bl_label = "Toggle VRPN Streaming"

	def execute(self, context):
		if vrpn_streaming:
			vrpn_stopStreaming(context)

			try: bpy.app.timers.unregister(updateTrackerStatesTimer)
			except: print("Failed to unregister timer!")
		else:
			vrpn_startStreaming(context)

			recording_database = None
			recording_startFrame = None
			recording_startTime = None

			settings = context.scene.VRPNSettings
			for tracker in settings.trackers:
				target = getTrackerTarget(tracker)
				if not target: continue
				posFC, rotFC = getTargetFCurves(target)
				for i in range(0,len(posFC)):
					posFC[i].keyframe_points.clear()
				for i in range(0,len(rotFC)):
					rotFC[i].keyframe_points.clear()

			bpy.app.timers.register(updateTrackerStatesTimer)

		return {'FINISHED'}

class ApplyRecording(bpy.types.Operator):
	bl_idname = "vrpn.apply_recording"
	bl_label = "Apply Internal Recording to Keyframes"
	bl_options = {'REGISTER', 'UNDO'}

	def execute(self, context):
		if not recording_database:
			return { 'CANCELLED' }

		settings = context.scene.VRPNSettings
		for tracker in settings.trackers:
			target = getTrackerTarget(tracker)
			if not target:
				print("No target for tracker {}!".format(tracker.label))
				continue
			if not tracker.vrpnPath in recording_database:
				print("No internal recording for tracker {}!".format(tracker.label))
				continue
			db = recording_database[tracker.vrpnPath]

			old_rot = target.rotation_quaternion.copy() if target.rotation_mode == 'QUATERNION' else target.rotation_euler.copy()

			posArr = [
				array.array('f', [0])*(2*len(db)),
				array.array('f', [0])*(2*len(db)),
				array.array('f', [0])*(2*len(db))
			]
			rotArr = [
				array.array('f', [0])*(2*len(db)),
				array.array('f', [0])*(2*len(db)),
				array.array('f', [0])*(2*len(db)),
				array.array('f', [0])*(2*len(db))
			]

			print("Adding {} keyframes to tracker {}!".format(len(db), tracker.label))

			for i, record in enumerate(db):
				# Prepare transform values
				tgt_mat = getTargetTransform(tracker, target, record.mat)
				pos = tgt_mat.to_translation()
				if target.rotation_mode == 'QUATERNION':
					rot = tgt_mat.to_quaternion()
					rot.make_compatible(old_rot)
				else:
					rot = tgt_mat.to_euler(target.rotation_euler.order, old_rot)
				old_rot = rot

				for j, d in enumerate(pos):
					posArr[j][i*2+0] = record.frame
					posArr[j][i*2+1] = d
				for j, d in enumerate(rot):
					rotArr[j][i*2+0] = record.frame
					rotArr[j][i*2+1] = d

			posFC, rotFC = getTargetFCurves(target)
			for i in range(0,len(posFC)):
				posFC[i].keyframe_points.clear()
				#pre = len(posFC[i].keyframe_points)
				posFC[i].keyframe_points.add(len(db))
				#for j in range(pre, len(posFC[i].keyframe_points)):
				#	setattr(posFC[i].keyframe_points[j], "co", some_seq[i])
				posFC[i].keyframe_points.foreach_set("co", posArr[i])
			for i in range(0,len(rotFC)):
				rotFC[i].keyframe_points.clear()
				rotFC[i].keyframe_points.add(len(db))
				rotFC[i].keyframe_points.foreach_set("co", rotArr[i])
			
		return {'FINISHED'}

class RequestTracking(bpy.types.Operator):
	bl_idname = "vrpn.request_tracking"
	bl_label = "Request tracking for a specific time"

	reqTimeISO: bpy.props.StringProperty()
	reqFrame: bpy.props.FloatProperty()

	def invoke(self, context, event):
		if recording_startFrame is None:
			print("Requesting tracking data but no tracking recording is available!")
			return { 'CANCELLED' }
		time = datetime.fromisoformat(self.reqTimeISO)
		recTime, self.reqFrame = mapRecordingTime(time)
		print("Requesting tracking for frame {:.2f}, {:.2f}ms ago!".format(self.reqFrame, (datetime.now(timezone.utc)-time)/timedelta(milliseconds=1)))
		return self.execute(context)

	def execute(self, context):
		if not recording_database:
			return { 'CANCELLED' }
		settings = context.scene.VRPNSettings
		for tracker in settings.trackers:
			target = getTrackerTarget(tracker)
			if not target:
				print("No target for tracker {}!".format(tracker.label))
				continue
			if not tracker.vrpnPath in recording_database:
				print("No internal recording for tracker {}!".format(tracker.label))
				continue
			db = recording_database[tracker.vrpnPath]
			index = bisect.bisect_left(db, self.reqFrame, key=lambda rec: rec.frame)
			frames = []
			if index > 0: frames.append(index-1)
			if index < len(db): frames.append(index)
			for f in frames:
				record = db[f]
				applyTargetTransform(target, getTargetTransform(tracker, target, record.mat))
				target.keyframe_insert('location', frame=record.frame)
				if target.rotation_mode == 'QUATERNION':
					target.keyframe_insert('rotation_quaternion', frame=record.frame)
				else:
					target.keyframe_insert('rotation_euler', frame=record.frame)
		context.scene.frame_float = self.reqFrame

		return {'FINISHED'}

class AddTracker(bpy.types.Operator):
	bl_idname = "vrpn.add_tracker"
	bl_label = "Add a new VRPN Tracker"
	bl_options = {'REGISTER', 'UNDO'}

	def execute(self, context):
		settings = context.scene.VRPNSettings
		tracker = settings.trackers.add()
		settings.active_tracker = min(max(settings.active_tracker, 0), len(settings.trackers)-1)
		return {'FINISHED'}

class RemoveTracker(bpy.types.Operator):
	bl_idname = "vrpn.remove_tracker"
	bl_label = "Remove the selected VRPN Tracker"
	bl_options = {'REGISTER', 'UNDO'}

	def execute(self, context):
		settings = context.scene.VRPNSettings
		if len(settings.trackers) == 0: return {'CANCELLED'}
		# Remove element
		removeIndex = min(max(settings.active_tracker, 0), len(settings.trackers)-1)
		removeID = settings.trackers[removeIndex].vrpnPath
		with vrpn_lock:
			if removeID in vrpn_trackers:
				vrpn_trackers.pop(removeID)
		settings.trackers.remove(removeIndex)
		# Update active index
		settings.active_tracker = min(max(removeIndex-1, 0), len(settings.trackers)-1)
		return {'FINISHED'}

class VRPNStreamingPanel(bpy.types.Panel):
	bl_category = "VRPN"
	bl_label = "VRPN Streaming"
	bl_idname = "VRPN_PT_Streaming"
	bl_space_type = "VIEW_3D"
	bl_region_type = "UI"

	def draw(self, context):
		layout = self.layout

		settings = context.scene.VRPNSettings

		label = 'Stop Streaming' if vrpn_streaming else 'Start Streaming'
		layout.operator(ToggleStreaming.bl_idname, text=label)

		row = layout.row()
		row.prop(settings, 'apply_pose_directly')
		row.enabled = settings.record_keyframes is False
		layout.prop(settings, 'record_keyframes')
		layout.prop(settings, 'record_internally')

		row = layout.row()
		row.enabled = recording_database is not None
		row.operator(ApplyRecording.bl_idname, text="Apply Recording to Keyframes")

		row = layout.row()
		row.template_list("VRPN_UL_trackers", "", settings, "trackers", settings, "active_tracker", rows=len(settings.trackers))

		col = row.column(align=True)
		col.operator(AddTracker.bl_idname, icon='ADD', text="")
		col.operator(RemoveTracker.bl_idname, icon='REMOVE', text="")

		if len(settings.trackers) > 0:
			active_tracker = min(max(settings.active_tracker, 0), len(settings.trackers)-1)
			tracker = settings.trackers[active_tracker]
			hasTracker = tracker.vrpnPath in vrpn_trackers # Might need a lock here
			if hasTracker:
				layout.label(text=tracker.vrpnPath)
			else:
				layout.prop(tracker, 'vrpnPath')
			layout.prop_search(tracker, 'target', context.scene, 'objects')
			if tracker.target:
				item = bpy.data.objects[tracker.target]
				if item.type == 'ARMATURE':
					row = layout.row()
					row.prop(tracker, 'use_subtarget')
					if tracker.use_subtarget:
						row.prop_search(tracker, 'subtarget', item.pose, 'bones', text='')
						if not tracker.subtarget: return
						item = item.pose.bones[t.subtarget]
				else:
					layout.prop_search(tracker, 'origin', context.scene, 'objects')

				layout.prop(tracker, 'use_local_offset')
				if tracker.use_local_offset:
					row = layout.row(align=True)
					col = row.column(align=True)
					col.prop(tracker, 'local_pos')
					col = row.column()
					col.prop(tracker, 'local_rot')
					col = row.column()
					col.prop(tracker, 'local_scale')
		else:
			layout.label(text="No Tracker selected! Have {:d}".format(len(settings.trackers)))

class VRPN_UL_trackers(bpy.types.UIList):
	def draw_item(self, context, layout, data, item, icon, active_data, active_propname):
		settings = data
		tracker = item
		if vrpn_streaming:
			hasTracker = tracker.vrpnPath in vrpn_trackers
			hasReceived = vrpn_trackers[tracker.vrpnPath].hasReceived if hasTracker else False
			n = 'TRIA_RIGHT' if hasReceived else ('QUESTION' if hasTracker else 'CANCEL')
			icon = bpy.types.UILayout.bl_rna.functions["prop"].parameters["icon"].enum_items[n].value
		if self.layout_type in {'COMPACT','DEFAULT'}:
			layout.prop(tracker, 'label',text="",emboss=False,icon_value=icon)
		elif self.layout_type in {'GRID'}:
			layout.label(text="",translate=False,icon_value=icon)

class VRPNTrackerSetup(bpy.types.PropertyGroup):
	label: bpy.props.StringProperty(name='Label', default="New Tracker")
	vrpnPath: bpy.props.StringProperty(name='Path', default="tracker_001")

	# Target object to apply tracking data to
	target: bpy.props.StringProperty(name='Target')

	# For normal targets
	origin: bpy.props.StringProperty(name='Origin')

	# For targets in armatures
	use_subtarget: bpy.props.BoolProperty(name="Use Bone", default=True)
	subtarget: bpy.props.StringProperty(name='Subtarget')

	# TODO: Remove
	use_local_offset: bpy.props.BoolProperty(name="Use Offset", default=True)
	local_pos: bpy.props.FloatVectorProperty(name="Pos")
	local_rot: bpy.props.FloatVectorProperty(name="Rot")
	local_scale: bpy.props.FloatVectorProperty(name="Scale", default=(1,1,1))

class VRPNSettings(bpy.types.PropertyGroup):
	trackers: bpy.props.CollectionProperty(type=VRPNTrackerSetup)
	active_tracker: bpy.props.IntProperty(name='Active Tracker', default=0)

	apply_pose_directly: bpy.props.BoolProperty(name="Apply Pose Directly", default=True)
	record_keyframes: bpy.props.BoolProperty(name="Record Keyframes", default=False)
	record_internally: bpy.props.BoolProperty(name="Record Internally", default=False)

def register():
	bpy.utils.register_class(VRPNStreamingPanel)
	bpy.utils.register_class(VRPNTrackerSetup)
	bpy.utils.register_class(VRPNSettings)

	bpy.utils.register_class(VRPN_UL_trackers)
	bpy.utils.register_class(ToggleStreaming)
	bpy.utils.register_class(ApplyRecording)
	bpy.utils.register_class(RequestTracking)
	bpy.utils.register_class(AddTracker)
	bpy.utils.register_class(RemoveTracker)

	bpy.types.Scene.VRPNSettings = bpy.props.PointerProperty(type=VRPNSettings)

def unregister():
	if vrpn_streaming:
		vrpn_stopStreaming()

	bpy.utils.unregister_class(VRPNStreamingPanel)
	bpy.utils.unregister_class(VRPNTrackerSetup)
	bpy.utils.unregister_class(VRPNSettings)

	bpy.utils.unregister_class(VRPN_UL_trackers)
	bpy.utils.unregister_class(ToggleStreaming)
	bpy.utils.unregister_class(ApplyRecording)
	bpy.utils.unregister_class(RequestTracking)
	bpy.utils.unregister_class(AddTracker)
	bpy.utils.unregister_class(RemoveTracker)

if __name__ == "__main__":
	register()