/**	
 * Copyright (C) Johannes Elliesen, 2020
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
