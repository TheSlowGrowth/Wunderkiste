#ifndef STATEMACHINE_H_
#define STATEMACHINE_H_

typedef enum 
{
    stInit = 0,
    stLinkSearch,
    stLinkWaitingForTag,
    stLinkWaitingForTagRemove,
    stWaitingForTag,
    stPlaying,
    stStoppedWaitingForTagRemove
} machineState_t;

void stateMachine_init();
void stateMachine_StorageAdded();
void stateMachine_StorageRemoved();
void stateMachine_run();
machineState_t stateMachine_getState();
void stateMachine_playbackFinished();

#endif // ifndef STATEMACHINE_H_
