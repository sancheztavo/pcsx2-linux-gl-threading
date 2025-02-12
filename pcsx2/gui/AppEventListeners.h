/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "common/EventSource.h"
#include "common/pxEvents.h"
#include "common/IniInterface.h"

enum CoreThreadStatus
{
	CoreThread_Indeterminate,
	CoreThread_Started,
	CoreThread_Resumed,
	CoreThread_Suspended,
	CoreThread_Reset,
	CoreThread_Stopped,
};

enum AppEventType
{
	AppStatus_UiSettingsLoaded,
	AppStatus_UiSettingsSaved,
	AppStatus_VmSettingsLoaded,
	AppStatus_VmSettingsSaved,

	AppStatus_SettingsApplied,
	AppStatus_Exiting
};

struct AppEventInfo
{
	AppEventType	evt_type;

	AppEventInfo( AppEventType type )
	{
		evt_type = type;
	}
};

struct AppSettingsEventInfo : AppEventInfo
{
	IniInterface&	m_ini;

	AppSettingsEventInfo( IniInterface&	ini, AppEventType evt_type );

	IniInterface& GetIni() const
	{
		return const_cast<IniInterface&>(m_ini);
	}
};

// --------------------------------------------------------------------------------------
//  IEventListener_CoreThread
// --------------------------------------------------------------------------------------
class IEventListener_CoreThread : public IEventDispatcher<CoreThreadStatus>
{
public:
	typedef CoreThreadStatus EvtParams;

public:
	virtual ~IEventListener_CoreThread() = default;

	virtual void DispatchEvent( const CoreThreadStatus& status );

protected:
	virtual void CoreThread_OnStarted() {}
	virtual void CoreThread_OnResumed() {}
	virtual void CoreThread_OnSuspended() {}
	virtual void CoreThread_OnReset() {}
	virtual void CoreThread_OnStopped() {}
};

class EventListener_CoreThread : public IEventListener_CoreThread
{
public:
	EventListener_CoreThread();
	virtual ~EventListener_CoreThread();
};

// --------------------------------------------------------------------------------------
//  IEventListener_AppStatus
// --------------------------------------------------------------------------------------
class IEventListener_AppStatus : public IEventDispatcher<AppEventInfo>
{
public:
	typedef AppEventInfo EvtParams;

public:
	virtual ~IEventListener_AppStatus() = default;

	virtual void DispatchEvent( const AppEventInfo& evtinfo );

protected:
	virtual void AppStatusEvent_OnUiSettingsLoadSave( const AppSettingsEventInfo& evtinfo ) {}
	virtual void AppStatusEvent_OnVmSettingsLoadSave( const AppSettingsEventInfo& evtinfo ) {}

	virtual void AppStatusEvent_OnSettingsApplied() {}
	virtual void AppStatusEvent_OnExit() {}
};

class EventListener_AppStatus : public IEventListener_AppStatus
{
public:
	EventListener_AppStatus();
	virtual ~EventListener_AppStatus();
};

// --------------------------------------------------------------------------------------
//  EventListenerHelpers (CoreThread / AppStatus)
// --------------------------------------------------------------------------------------
// Welcome to the awkward world of C++ multi-inheritence.  wxWidgets' Connect() system is
// incompatible because of limitations in C++ class member function pointers, so we need
// this second layer class to act as a bridge between the event system and the class's
// handler implementations.
//
// Explained in detail: The class that wants to listen to shit will implement its expected
// virtual overrides from the listener classes (OnCoreThread_Started, for example), and then
// it adds an instance of the EventListenerHelper_CoreThread class to itself, instead of
// *inheriting* from it.  Thusly, the Helper gets initialized when the class is created,
// and when events are dispatched to the listener, it forwards the event to the main class.
//  --air

template< typename TypeToDispatchTo >
class EventListenerHelper_CoreThread : public EventListener_CoreThread
{
public:
	TypeToDispatchTo&	Owner;

public:
	EventListenerHelper_CoreThread( TypeToDispatchTo& dispatchTo )
		: Owner( dispatchTo ) { }

	EventListenerHelper_CoreThread( TypeToDispatchTo* dispatchTo )
		: Owner( *dispatchTo )
	{
		pxAssert(dispatchTo != NULL);
	}

	virtual ~EventListenerHelper_CoreThread() = default;

protected:
	void CoreThread_OnIndeterminate()	{ Owner.OnCoreThread_Indeterminate(); }
	void CoreThread_OnStarted()			{ Owner.OnCoreThread_Started(); }
	void CoreThread_OnResumed()			{ Owner.OnCoreThread_Resumed(); }
	void CoreThread_OnSuspended()		{ Owner.OnCoreThread_Suspended(); }
	void CoreThread_OnReset()			{ Owner.OnCoreThread_Reset(); }
	void CoreThread_OnStopped()			{ Owner.OnCoreThread_Stopped(); }
};

template< typename TypeToDispatchTo >
class EventListenerHelper_AppStatus : public EventListener_AppStatus
{
public:
	TypeToDispatchTo&	Owner;

public:
	EventListenerHelper_AppStatus( TypeToDispatchTo& dispatchTo )
		: Owner( dispatchTo ) { }

	EventListenerHelper_AppStatus( TypeToDispatchTo* dispatchTo )
		: Owner( *dispatchTo )
	{
		pxAssert(dispatchTo != NULL);
	}

	virtual ~EventListenerHelper_AppStatus() = default;

protected:
	virtual void AppStatusEvent_OnUiSettingsLoadSave( const AppSettingsEventInfo& evtinfo ) { Owner.AppStatusEvent_OnUiSettingsLoadSave( evtinfo ); }
	virtual void AppStatusEvent_OnVmSettingsLoadSave( const AppSettingsEventInfo& evtinfo ) { Owner.AppStatusEvent_OnVmSettingsLoadSave( evtinfo ); }
	virtual void AppStatusEvent_OnSettingsApplied() { Owner.AppStatusEvent_OnSettingsApplied(); }
	virtual void AppStatusEvent_OnExit() { Owner.AppStatusEvent_OnExit(); }
};


// --------------------------------------------------------------------------------------
//  CoreThreadStatusEvent
// --------------------------------------------------------------------------------------
class CoreThreadStatusEvent : public pxActionEvent
{
	typedef pxActionEvent _parent;

protected:
	CoreThreadStatus		m_evt;

public:
	virtual ~CoreThreadStatusEvent() = default;
	CoreThreadStatusEvent* Clone() const { return new CoreThreadStatusEvent( *this ); }

	explicit CoreThreadStatusEvent( CoreThreadStatus evt, SynchronousActionState* sema=NULL );
	explicit CoreThreadStatusEvent( CoreThreadStatus evt, SynchronousActionState& sema );

	void SetEventType( CoreThreadStatus evt ) { m_evt = evt; }
	CoreThreadStatus GetEventType() { return m_evt; }

protected:
	void InvokeEvent();
};
