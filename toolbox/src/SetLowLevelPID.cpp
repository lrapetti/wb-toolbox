#include "SetLowLevelPID.h"
#include "BlockInformation.h"
#include "Log.h"
#include "RobotInterface.h"

#include <yarp/dev/ControlBoardPid.h>

#include <algorithm>

using namespace wbt;

const std::string SetLowLevelPID::ClassName = "SetLowLevelPID";

const unsigned PARAM_IDX_BIAS = WBBlock::NumberOfParameters - 1;
const unsigned PARAM_IDX_PIDCONFIG = PARAM_IDX_BIAS + 1;
const unsigned PARAM_IDX_CTRL_TYPE = PARAM_IDX_BIAS + 2;

enum PidDataIndex
{
    PGAIN = 0,
    IGAIN = 1,
    DGAIN = 2
};

unsigned SetLowLevelPID::numberOfParameters()
{
    return WBBlock::numberOfParameters() + 2;
}

bool SetLowLevelPID::parseParameters(BlockInformation* blockInfo)
{
    ParameterMetadata paramMD_P_cell(PARAM_STRUCT_CELL_DOUBLE, PARAM_IDX_PIDCONFIG, 1, 1, "P");
    ParameterMetadata paramMD_I_cell(PARAM_STRUCT_CELL_DOUBLE, PARAM_IDX_PIDCONFIG, 1, 1, "I");
    ParameterMetadata paramMD_D_cell(PARAM_STRUCT_CELL_DOUBLE, PARAM_IDX_PIDCONFIG, 1, 1, "D");
    ParameterMetadata paramMD_ctrlType_cell(PARAM_STRING, PARAM_IDX_CTRL_TYPE, 1, 1, "ControlType");

    ParameterMetadata paramMD_jointList_cell(
        PARAM_CELL_STRING, PARAM_IDX_PIDCONFIG, 1, 1, "jointList");

    bool ok = true;
    ok = ok && blockInfo->addParameterMetadata(paramMD_P_cell);
    ok = ok && blockInfo->addParameterMetadata(paramMD_I_cell);
    ok = ok && blockInfo->addParameterMetadata(paramMD_D_cell);
    ok = ok && blockInfo->addParameterMetadata(paramMD_jointList_cell);
    ok = ok && blockInfo->addParameterMetadata(paramMD_ctrlType_cell);

    if (!ok) {
        wbtError << "Failed to store parameters metadata.";
        return false;
    }

    return blockInfo->parseParameters(m_parameters);
}

bool SetLowLevelPID::configureSizeAndPorts(BlockInformation* blockInfo)
{
    if (!WBBlock::configureSizeAndPorts(blockInfo)) {
        return false;
    }

    // INPUTS
    // ======
    //
    // No inputs
    //

    if (!blockInfo->setNumberOfInputPorts(0)) {
        wbtError << "Failed to configure the number of input ports.";
        return false;
    }

    // OUTPUTS
    // =======
    //
    // No outputs
    //

    if (!blockInfo->setNumberOfOutputPorts(0)) {
        wbtError << "Failed to configure the number of output ports.";
        return false;
    }

    return true;
}

bool SetLowLevelPID::initialize(BlockInformation* blockInfo)
{
    if (!WBBlock::initialize(blockInfo)) {
        return false;
    }

    // INPUT PARAMETERS
    // ================

    if (!parseParameters(blockInfo)) {
        wbtError << "Failed to parse parameters.";
        return false;
    }

    // Reading the control type
    std::string controlType;
    std::vector<double> p_Gains;
    std::vector<double> i_Gains;
    std::vector<double> d_Gains;
    std::vector<std::string> pidJointList;

    bool ok = true;
    ok = ok && m_parameters.getParameter("ControlType", controlType);
    ok = ok && m_parameters.getParameter("P", p_Gains);
    ok = ok && m_parameters.getParameter("I", i_Gains);
    ok = ok && m_parameters.getParameter("D", d_Gains);
    ok = ok && m_parameters.getParameter("jointList", pidJointList);

    if (!ok) {
        wbtError << "Failed to get parameters after their parsing.";
        return false;
    }

    // Create a map {joint => PID} from the parameters
    // ===============================================

    // Get the DoFs
    const auto robotInterface = getRobotInterface(blockInfo).lock();
    if (!robotInterface) {
        wbtError << "RobotInterface has not been correctly initialized.";
        return false;
    }
    const auto& configuration = robotInterface->getConfiguration();
    const auto& controlledJoints = configuration.getControlledJoints();

    // Pupulate the map
    for (unsigned i = 0; i < pidJointList.size(); ++i) {
        // Find if the joint of the processed PID is a controlled joint
        auto findElement = std::find(
            std::begin(controlledJoints), std::end(controlledJoints), controlledJoints[i]);
        // Edit the PID for this joint
        if (findElement != std::end(controlledJoints)) {
            m_pidJointsFromParameters.emplace(pidJointList[i],
                                              std::make_tuple(p_Gains[i], i_Gains[i], d_Gains[i]));
        }
        else {
            wbtWarning << "Attempted to set PID of joint " << controlledJoints[i]
                       << " which is not currently controlled. Skipping it.";
        }
    }

    // Configure the class
    // ===================

    if (controlType == "Position") {
        m_controlType = yarp::dev::VOCAB_PIDTYPE_POSITION;
    }
    else if (controlType == "Torque") {
        m_controlType = yarp::dev::VOCAB_PIDTYPE_TORQUE;
    }
    else {
        wbtError << "Control type not recognized.";
        return false;
    }

    // Retain the RemoteControlBoardRemapper
    if (!robotInterface->retainRemoteControlBoardRemapper()) {
        wbtError << "Couldn't retain the RemoteControlBoardRemapper.";
        return false;
    }

    const auto dofs = configuration.getNumberOfDoFs();

    // Initialize the vector size to the number of dofs
    m_defaultPidValues.resize(dofs);

    // Get the interface
    yarp::dev::IPidControl* iPidControl = nullptr;
    if (!robotInterface->getInterface(iPidControl) || !iPidControl) {
        wbtError << "Failed to get IPidControl interface.";
        return false;
    }

    // Store the default gains
    if (!iPidControl->getPids(m_controlType, m_defaultPidValues.data())) {
        wbtError << "Failed to get default data from IPidControl.";
        return false;
    }

    // Initialize the vector of the applied pid gains with the default gains
    m_appliedPidValues = m_defaultPidValues;

    // Override the PID with the gains specified as block parameters
    for (unsigned i = 0; i < dofs; ++i) {
        const std::string& jointName = controlledJoints[i];
        // If the pid has been passed, set the new gains
        if (m_pidJointsFromParameters.find(jointName) != m_pidJointsFromParameters.end()) {
            PidData gains = m_pidJointsFromParameters[jointName];
            m_appliedPidValues[i].setKp(std::get<PGAIN>(gains));
            m_appliedPidValues[i].setKi(std::get<IGAIN>(gains));
            m_appliedPidValues[i].setKd(std::get<DGAIN>(gains));
        }
    }

    // Apply the new pid gains
    if (!iPidControl->setPids(m_controlType, m_appliedPidValues.data())) {
        wbtError << "Failed to set PID values.";
        return false;
    }

    return true;
}

bool SetLowLevelPID::terminate(const BlockInformation* blockInfo)
{
    bool ok = true;

    // Get the IPidControl interface
    yarp::dev::IPidControl* iPidControl = nullptr;
    ok = ok && getRobotInterface()->getInterface(iPidControl);
    if (!ok || !iPidControl) {
        wbtError << "Failed to get IPidControl interface.";
        // Don't return false here. WBBlock::terminate must be called in any case
    }

    // Reset default pid gains
    ok = ok && iPidControl->setPids(m_controlType, m_defaultPidValues.data());
    if (!ok) {
        wbtError << "Failed to reset PIDs to the default values.";
        // Don't return false here. WBBlock::terminate must be called in any case
    }

    // Release the RemoteControlBoardRemapper
    ok = ok && getRobotInterface()->releaseRemoteControlBoardRemapper();
    if (!ok) {
        wbtError << "Failed to release the RemoteControlBoardRemapper.";
        // Don't return false here. WBBlock::terminate must be called in any case
    }

    return ok && WBBlock::terminate(blockInfo);
}

bool SetLowLevelPID::output(const BlockInformation* blockInfo)
{
    return true;
}
