////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (c) 2016-2017 Leonardo Consoni <consoni_2519@hotmail.com>       //
//                                                                            //
//  This file is part of RobRehabSystem.                                      //
//                                                                            //
//  RobRehabSystem is free software: you can redistribute it and/or modify    //
//  it under the terms of the GNU Lesser General Public License as published  //
//  by the Free Software Foundation, either version 3 of the License, or      //
//  (at your option) any later version.                                       //
//                                                                            //
//  RobRehabSystem is distributed in the hope that it will be useful,         //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of            //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              //
//  GNU Lesser General Public License for more details.                       //
//                                                                            //
//  You should have received a copy of the GNU Lesser General Public License  //
//  along with RobRehabSystem. If not, see <http://www.gnu.org/licenses/>.    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <string.h>

#include "robot_control_interface.h"

#include "debug/data_logging.h"

#define DOFS_NUMBER 2

const double CONTROL_TOLERANCE = 0.001;

typedef struct _ControllerData
{
  bool axesChangedList[ DOFS_NUMBER ];
  bool jointsChangedList[ DOFS_NUMBER ];
  double lastAxisPositionsList[ DOFS_NUMBER ];
  double lastJointPositionsList[ DOFS_NUMBER ];
  double estimatedInertiasList[ DOFS_NUMBER ];
  double maxFrictionForcesList[ DOFS_NUMBER ];
  enum RobotState controlState;
  double samplingTime;
  Log samplingLog;
}
ControlData;

const char* DOF_NAMES[ DOFS_NUMBER ] = { "angle1", "angle2" };


DECLARE_MODULE_INTERFACE( ROBOT_CONTROL_INTERFACE );


RobotController InitController( const char* configurationString )
{
  ControlData* newController = (ControlData*) malloc( sizeof(ControlData) );
  memset( newController, 0, sizeof(ControlData) );
  
  Log_SetBaseDirectory( "" );
  newController->samplingLog = Log_Init( "motor_sampling", 8 );
  
  newController->samplingTime = 0.0;
  
  sscanf( configurationString, "%lf %lf", &(newController->estimatedInertiasList[ 0 ]), &(newController->estimatedInertiasList[ 1 ]) );
  
  newController->controlState = ROBOT_PASSIVE;
  
  return newController;
}

void EndController( RobotController ref_controller )
{
  if( ref_controller == NULL ) return;
  
  ControlData* controller = (ControlData*) ref_controller;
    
  Log_End( controller->samplingLog );
    
  free( ref_controller );
}

size_t GetJointsNumber( RobotController controller )
{
  return DOFS_NUMBER;
}

const char** GetJointNamesList( RobotController ref_controller )
{
  return (const char**) DOF_NAMES;
}

const bool* GetJointsChangedList( RobotController ref_controller )
{
  if( ref_controller == NULL ) return NULL;
  
  ControlData* controller = (ControlData*) ref_controller;
  
  return (const bool*) controller->jointsChangedList;
}

size_t GetAxesNumber( RobotController ref_controller )
{
  return DOFS_NUMBER;
}

const char** GetAxisNamesList( RobotController ref_controller )
{
  return (const char**) DOF_NAMES;
}

const bool* GetAxesChangedList( RobotController ref_controller )
{
  if( ref_controller == NULL ) return NULL;
  
  ControlData* controller = (ControlData*) ref_controller;
  
  return (const bool*) controller->axesChangedList;
}

void SetControlState( RobotController ref_controller, enum RobotState newControlState )
{
  if( ref_controller == NULL ) return;
  ControlData* controller = (ControlData*) ref_controller;
  
  fprintf( stderr, "Setting robot control phase: %x\n", newControlState );
  
  if( newControlState == ROBOT_PREPROCESSING )
  {
    controller->samplingTime = 0.0;
    controller->maxFrictionForcesList[ 0 ] = controller->maxFrictionForcesList[ 1 ] = 0.0;
  }
  
  controller->controlState = newControlState;
}

void ControlJoint( RobotVariables* ref_jointMeasures, RobotVariables* ref_axisMeasures, RobotVariables* ref_jointSetpoints, RobotVariables* ref_axisSetpoints, double timeDelta )
{
  ref_axisMeasures->acceleration = ref_jointMeasures->acceleration;
  ref_axisMeasures->velocity = ref_jointMeasures->velocity;
  ref_axisMeasures->position = ref_jointMeasures->position;
  ref_axisMeasures->force = ref_axisMeasures->inertia * ref_axisMeasures->acceleration - ref_jointMeasures->force;
  ref_axisMeasures->stiffness = ref_jointMeasures->stiffness;
  ref_axisMeasures->damping = ref_jointMeasures->damping;
  
  ref_jointSetpoints->velocity = ref_axisSetpoints->velocity;                           // xdot_d
  ref_jointSetpoints->position = ref_axisSetpoints->position;                           // x_d
  ref_jointSetpoints->acceleration = ref_axisSetpoints->acceleration;
  ref_jointSetpoints->force = ref_axisSetpoints->force;
  ref_jointSetpoints->stiffness = ref_axisSetpoints->stiffness;                         // K = lamda^2 * m
  ref_jointSetpoints->damping = ref_axisSetpoints->damping;                             // B = D = lamda * m
  
  //double positionError = ref_jointSetpoints->position - ref_jointMeasures->position;    // e_p = x_d - x
  //double velocityError = ref_jointSetpoints->velocity - ref_jointMeasures->velocity;    // e_v = xdot_d - xdot
  // F_actuator = K * e_p + B * e_v - D * x_dot
  //double controlForce = ref_jointSetpoints->stiffness * positionError - ref_jointSetpoints->damping * velocityError;
  //double dampingForce = ref_jointSetpoints->damping * ref_jointMeasures->velocity;
  //ref_jointSetpoints->force += controlForce - dampingForce;
}

bool CheckAxisStateChange( RobotVariables* ref_axisMeasures, double* lastAxisPosition )
{
  if( fabs( ref_axisMeasures->position - (*lastAxisPosition) ) > 0.01 || fabs( ref_axisMeasures->force ) > 0.05 )
  {
    *lastAxisPosition = ref_axisMeasures->position;
    return true;
  }
  
  return false;
}

bool CheckJointStateChange( RobotVariables* ref_jointMeasures, double* lastJointPosition )
{
  if( fabs( ref_jointMeasures->position - (*lastJointPosition) ) > 0.01 || fabs( ref_jointMeasures->force ) > 0.05 )
  {
    *lastJointPosition = ref_jointMeasures->position;
    return true;
  }
  
  return false;
}

const double MIN_FREQUENCY = 1.0, MAX_FREQUENCY = 30.0;
const double TOTAL_SAMPLING_TIME = 20.0;
const double FREQUENCY_SLOPE = ( MAX_FREQUENCY - MIN_FREQUENCY ) / TOTAL_SAMPLING_TIME;
void RunControlStep( RobotController ref_controller, RobotVariables** jointMeasuresList, RobotVariables** axisMeasuresList, 
                                                     RobotVariables** jointSetpointsList, RobotVariables** axisSetpointsList, double timeDelta )
{
  if( ref_controller == NULL ) return;
  ControlData* controller = (ControlData*) ref_controller;
  
  for( size_t dofIndex = 0; dofIndex < DOFS_NUMBER; dofIndex++ )
  {
    axisMeasuresList[ dofIndex ]->inertia = controller->estimatedInertiasList[ dofIndex ];
    axisSetpointsList[ dofIndex ]->stiffness = axisMeasuresList[ dofIndex ]->stiffness = controller->estimatedInertiasList[ dofIndex ];
    axisSetpointsList[ dofIndex ]->damping = axisMeasuresList[ dofIndex ]->damping = controller->estimatedInertiasList[ dofIndex ];
  }
  
  ControlJoint( jointMeasuresList[ 0 ], axisMeasuresList[ 0 ], jointSetpointsList[ 0 ], axisSetpointsList[ 0 ], timeDelta );
  
  if( controller->controlState == ROBOT_PREPROCESSING )
  {
    Log_EnterNewLine( controller->samplingLog, controller->samplingTime );
    Log_RegisterValues( controller->samplingLog, 4, jointMeasuresList[ 0 ]->force, jointMeasuresList[ 0 ]->position, jointMeasuresList[ 0 ]->velocity, jointMeasuresList[ 0 ]->acceleration );
    
    double frequency = FREQUENCY_SLOPE * ( fmod( controller->samplingTime, TOTAL_SAMPLING_TIME ) ) + MIN_FREQUENCY;
    jointSetpointsList[ 0 ]->force = 0.005 * cos( frequency * controller->samplingTime );
    
    controller->samplingTime += timeDelta;
  }
  
  controller->axesChangedList[ 0 ] = CheckAxisStateChange( axisMeasuresList[ 0 ], &(controller->lastAxisPositionsList[ 0 ]) );
  controller->jointsChangedList[ 0 ] = CheckJointStateChange( jointMeasuresList[ 0 ], &(controller->lastJointPositionsList[ 0 ]) );
  
  //if( fabs( jointMeasuresList[ 0 ]->velocity ) < CONTROL_TOLERANCE )
  //{
  //  if( jointSetpointsList[ 0 ]->force > CONTROL_TOLERANCE ) jointSetpointsList[ 0 ]->force += controller->maxFrictionForcesList[ 0 ];
  //  else if( jointSetpointsList[ 0 ]->force < -CONTROL_TOLERANCE ) jointSetpointsList[ 0 ]->force -= controller->maxFrictionForcesList[ 0 ];
  //}
  
  fprintf( stderr, "positions: %.5f, %.5f | control forces: %.5f, %.5f\r", jointMeasuresList[ 0 ]->position, jointMeasuresList[ 1 ]->position, jointSetpointsList[ 0 ]->force, jointSetpointsList[ 1 ]->force );
  
  
  ControlJoint( jointMeasuresList[ 1 ], axisMeasuresList[ 1 ], jointSetpointsList[ 1 ], axisSetpointsList[ 1 ], timeDelta );
  
  controller->axesChangedList[ 1 ] = CheckAxisStateChange( axisMeasuresList[ 1 ], &(controller->lastAxisPositionsList[ 1 ]) );
  controller->jointsChangedList[ 1 ] = CheckJointStateChange( jointMeasuresList[ 1 ], &(controller->lastJointPositionsList[ 1 ]) );
  
  if( controller->controlState != ROBOT_OPERATION && controller->controlState != ROBOT_PREPROCESSING ) jointSetpointsList[ 0 ]->force = jointSetpointsList[ 1 ]->force = 0.0;
}