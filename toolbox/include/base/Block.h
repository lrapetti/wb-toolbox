#ifndef WBT_BLOCK_H
#define WBT_BLOCK_H

#include <string>
#include <vector>

namespace wbt {
    class Block;
    class BlockInformation;

}

/**
 * Basic abstract class for all the blocks.
 * This class (the whole toolbox in reality) assumes the block represent
 * an instantaneous system (i.e. not a dynamic system).
 *
 * You can create a new block by deriving this class and implementing at least
 * all the pure virtual methods.
 *
 * @Note: if you need to implement a block which uses the WBI, you should derive
 * from WBIBlock as it already provides some facilities.
 */
class wbt::Block
{
public:
    /**
     * Create and returns a new Block object of the specified class.
     *
     * If the class does not exist returns NULL
     * @param blockClassName the derived class name to be instantiated
     *
     * @return the newly created Block object or NULL.
     */
    static wbt::Block* instantiateBlockWithClassName(std::string blockClassName);

    /**
     * Destructor
     *
     */
    virtual ~Block();

    /**
     * Returns the number of configuration parameters needed by this block
     *
     * @return the number of parameters
     */
    virtual unsigned numberOfParameters() = 0;


    /**
     * Returns vector of additional block options
     *
     * @return vector containing a list of options
     */
    virtual std::vector<std::string> additionalBlockOptions();

    /**
     * Returns the number of discrete states of the block.
     *
     * The base implementation returns 0, i.e. no discrete states
     * @note if you return a number > 0, you should implement the
     * updateDiscreteState function
     * @return the number of discrete states
     */
    virtual unsigned numberOfDiscreteStates();

    /**
     * Returns the number of continuous states of the block.
     *
     * The base implementation returns 0, i.e. no continuous states
     * @note if you return a number > 0, you should implement the
     * stateDerivative function
     * @return the number of continuous states
     */
    virtual unsigned numberOfContinuousStates();

    /**
     * Called to update the internal discrete state
     *
     * i.e. x[i+1] = f(x[i])
     * @param S the SimStruct structure
     * @return true for success, false otherwise
     */
    virtual bool updateDiscreteState(const BlockInformation* blockInfo);

    /**
     * Not called for now
     *
     * @param S the SimStruct structure
     * @return true for success, false otherwise
     */
    virtual bool stateDerivative(const BlockInformation* blockInfo);


    /**
     * Specify if the parameter at the specified index is tunable
     *
     * Tunable means that it can be changed during the simulation.
     * @Note by default it specifies false for every parameter
     * @param [in]index   index of the parameter
     * @param [out]tunable true if the parameter is tunable. False otherwise
     */
    virtual void parameterAtIndexIsTunable(unsigned index, bool &tunable);

    /**
     * Configure the input and output ports
     *
     * This method is called at configuration time (i.e. during mdlInitializeSizes)
     * In this method you have to configure the number of input and output ports,
     * their size and configuration.
     * @Note: you should not save any object in this method because it will not persist
     * @param S     simulink structure
     *
     * @return true for success, false otherwise
     */
    virtual bool configureSizeAndPorts(BlockInformation* blockInfo) = 0;

    /**
     * Never called.
     *
     * @param S     simulink structure
     *
     * @return true for success, false otherwise
     */
    virtual bool checkParameters(const BlockInformation* blockInfo);

    /**
     * Initialize the object for the simulation
     *
     * This method is called at model startup (i.e. during mdlStart)
     * @Note: you can save and initialize your object in this method
     * @param S     simulink structure
     *
     * @return true for success, false otherwise
     */
    virtual bool initialize(const BlockInformation* blockInfo) = 0;

    /**
     * Called to initialize the initial conditions
     *
     * The default implementation do nothing.
     * @note this function is also called on a reset event
     * @note if you need to perform initialization only once, than implement initialize
     * @param S     simulink structure
     *
     * @return true for success, false otherwise
     */
    virtual bool initializeInitialConditions(const BlockInformation* blockInfo);

    /**
     * Perform model cleanup.
     *
     * This method is called at model termination (i.e. during mdlTerminate).
     * You should release all the resources you requested during the initialize method
     * @param S     simulink structure
     *
     * @return true for success, false otherwise
     */
    virtual bool terminate(const BlockInformation* blockInfo) = 0;



    /**
     * Compute the output of the block
     *
     * This method is called at every iteration of the model (i.e. during mdlOutput)
     * @param S     simulink structure
     * @param [out]error output error object that can be filled in case of error.
     *
     * @return true for success, false otherwise
     */
    virtual bool output(const BlockInformation* blockInfo) = 0;

public:

};

#endif /* end of include guard: WBT_BLOCK_H */
