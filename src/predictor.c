//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include "predictor.h"
#include <math.h>
//
// TODO:Student Information
//
const char *studentName = "Ashley Jacobs";
const char *studentID   = "A59008460";
const char *email       = "amjacobs@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here

// Both Gshare and Tournament
uint32_t GHR;
uint32_t* globalPredictionTable;
uint32_t numEntriesInGlobalPredictionTable;

// For Tournament
uint32_t* localPredictionTable; // 2 bit predictor
uint32_t numEntriesInLocalPredictionTable;
uint32_t* localHistoryTable; 
uint32_t numEntriesInLocalHistoryTable;
uint32_t* choicePredictionTable; 
uint32_t numEntriesInChoiceTable;

// For Perceptron
uint32_t **weightTable;
uint32_t *perceptronTable;
uint32_t numEntriesInPerceptronTables;
uint32_t numEntriesInWeightsVector;
uint32_t threshold;
//
uint32_t updatePrediction(uint32_t, uint8_t);
uint32_t updateChoiceTablePrediction(uint32_t, uint8_t, uint32_t);

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
// FUNCTION: Initializes the Gshare predictor's data structures
void init_gshare() {
  GHR= 0;
  numEntriesInGlobalPredictionTable = pow(2, ghistoryBits);
  globalPredictionTable = malloc(sizeof(uint32_t)*numEntriesInGlobalPredictionTable);
  for (int i = 0; i < numEntriesInGlobalPredictionTable ; i++){
    globalPredictionTable[i] = WN;
  }
}

// FUNCTION: Initializes the Tournament predictor's data structures
void init_tournament() {
  // Initialize global history components
  init_gshare();

  // Initialize local history components
  numEntriesInLocalPredictionTable = pow(2, lhistoryBits);
  localPredictionTable = malloc(sizeof(uint32_t)*numEntriesInLocalPredictionTable);
  for (int i = 0; i < numEntriesInLocalPredictionTable; i++){
    localPredictionTable[i] = WN;
  }

  numEntriesInLocalHistoryTable = pow(2, pcIndexBits);
  localHistoryTable = malloc(sizeof(uint32_t)*numEntriesInLocalHistoryTable);
  for (int i = 0; i < numEntriesInLocalHistoryTable; i++){
    localHistoryTable[i] = WN;
  }
  // Initialize choice prediction table
  numEntriesInChoiceTable = pow(2, ghistoryBits) ;
  choicePredictionTable = malloc(sizeof(uint32_t)*numEntriesInChoiceTable);
  for (int i = 0; i < numEntriesInChoiceTable; i++){
    choicePredictionTable[i] = SN;
  }
}

void init_perceptron() {
  // Size of data structures: 2^8*2^5 (perceptron table) + 2^8*(2^5*2^2) (weight table) + 2^5 (threshold) + 2^5 (GHR)
  // ^ A little over 13KB so should be good size-wise
  GHR = 0;
  threshold = pow(2, 5);
  numEntriesInPerceptronTables = pow(2, 8);
  // Use 2^5 bits max for vector of weights (subtract 1 due to much worse results in testing)
  numEntriesInWeightsVector = pow(2, 5) - 1;
  weightTable = malloc(sizeof(uint32_t*)*numEntriesInPerceptronTables);
  perceptronTable = malloc(sizeof(uint32_t)*numEntriesInPerceptronTables);
  for(int i=0; i<numEntriesInPerceptronTables; i++)
    weightTable[i] = malloc(sizeof(uint32_t)*numEntriesInWeightsVector); 
}

void
init_predictor()
{
  switch (bpType) {
    case GSHARE: 
      init_gshare();
      break;
    case TOURNAMENT:
      init_tournament();
      break;
    case CUSTOM:
      init_perceptron();
      break;
    default:
      break;
  }
}

int two_to_one_bit(uint32_t prediction) {
  if (prediction <= 1) {
    return NOTTAKEN;
  } else {
    return TAKEN;
  }
}

uint8_t make_tournament_prediction(uint32_t pc){
  uint8_t local_prediction = two_to_one_bit(localPredictionTable[localHistoryTable[pc%numEntriesInLocalHistoryTable]%numEntriesInLocalPredictionTable]);
  uint8_t global_prediction = two_to_one_bit(globalPredictionTable[GHR%numEntriesInGlobalPredictionTable]);
  if (local_prediction == global_prediction) {
    return global_prediction;
  }
  if (choicePredictionTable[GHR%numEntriesInChoiceTable] <= 1){
    // 0 and 1 represent l and L
    return local_prediction;
  }
  else {
    // 2 and 3 represent g and G
    return global_prediction;
  }
}

int y_value_calculation(uint32_t pc) {
	uint32_t w_0 = perceptronTable[pc % numEntriesInPerceptronTables];
  uint32_t y = w_0;
  // Weight table contains dot products needed to calculate y, so add that to the bias weight
	for (int i = 0; i < numEntriesInWeightsVector; i++) {
    // When the GHR bit to compute the dot product is 0, penalize rather than add 0 (since -y means not taken)
    int GHRbit = (GHR >> i) & 1;
    if (GHRbit == 1) {
      y += weightTable[pc%numEntriesInPerceptronTables][i]*(1);
    } 
    else {
      y += weightTable[pc%numEntriesInPerceptronTables][i]*(-1);
    } 
  }
  return y;
}

//Return TAKEN if the perceptron sum >=0, otherwise return NOTTAKEN
uint8_t make_perceptron_prediction(uint32_t pc) {
  if (y_value_calculation(pc) >= 0) {
    return TAKEN;
  }
  else {
    return NOTTAKEN;
  }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
  uint8_t make_prediction(uint32_t pc)
{
  //
  //TODO: Implement prediction scheme
  //
  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      if (globalPredictionTable[(pc^GHR)%numEntriesInGlobalPredictionTable] > 1) {
        return TAKEN;
      }
      else 
      {
        return NOTTAKEN;
      }
    case TOURNAMENT:
      return make_tournament_prediction(pc);
    case CUSTOM:
      return make_perceptron_prediction(pc);
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// FUNCTION: Train the Gshare predictor
void train_gshare(uint32_t pc, uint8_t outcome){
  uint32_t BHT_index = (pc^GHR)%numEntriesInGlobalPredictionTable;
  globalPredictionTable[BHT_index] = updatePrediction(globalPredictionTable[BHT_index], outcome);
  // Shift global history to add the new outcome
  GHR = ((GHR << 1) + outcome);
}

// FUNCTION: Train the tournament predictor
void train_tournament(uint32_t pc, uint8_t outcome){
  // Calculate the index used for each table
  uint32_t globalPredictionTable_index = GHR%numEntriesInGlobalPredictionTable;
  uint32_t localHistoryTable_index = pc%numEntriesInLocalHistoryTable;
  uint32_t localPredictionTable_index = localHistoryTable[pc%numEntriesInLocalHistoryTable]%numEntriesInLocalPredictionTable;
  uint32_t choiceTable_index = GHR%numEntriesInChoiceTable;

  // Calculate the local and global prediction values
  uint32_t local_prediction = localPredictionTable[localPredictionTable_index];
  uint32_t global_prediction = globalPredictionTable[globalPredictionTable_index];
  uint32_t choice_prediction = choicePredictionTable[choiceTable_index];
  
  // Update global history based on outcome
  globalPredictionTable[globalPredictionTable_index] = updatePrediction(global_prediction, outcome);
  localPredictionTable[localPredictionTable_index] = updatePrediction(local_prediction, outcome);
  localHistoryTable[localHistoryTable_index] = ((localHistoryTable[localHistoryTable_index] << 1) + outcome);

  // Update choice table
  if (two_to_one_bit(local_prediction) != two_to_one_bit(global_prediction)) {
    choicePredictionTable[choiceTable_index] = updateChoiceTablePrediction(choice_prediction, outcome, global_prediction);
  }

  // Shift global history to add the new outcome
  GHR = ((GHR << 1) + outcome);
}

int enforce_bit_length(int value, int bits)
{
  int val_limit = pow(2, bits-1)-1;
  int corrected = value;
  if (value > val_limit) {
    corrected--;
    }
  else if (value < -1*val_limit) {
    corrected++;
  }
  return corrected;
}

void train_perceptron(uint32_t pc, uint8_t outcome)
{
	int y_value = y_value_calculation(pc);
	uint8_t predictedOutcome = make_perceptron_prediction(pc);
	if (predictedOutcome != outcome || abs(y_value) <= threshold) {
    // Update perceptron table
    enforce_bit_length(perceptronTable[pc%numEntriesInPerceptronTables], 3);
    // Update weights
    for (int i = 0; i < numEntriesInWeightsVector; i++) {
      int GHRbit = (GHR >> i) & 1;
      // If global history predicted correctly, reward, else penalize
      if (GHRbit == outcome) {
        weightTable[pc%numEntriesInPerceptronTables][i] += 1;
      } 
      else {
        weightTable[pc%numEntriesInPerceptronTables][i] -= 1;
      }
      // Used 2 bits here only to enforce the size requirement
      // Ideally should use (1 + log2(threshold)) bits, but we still beat gshare/tournament in the tests
      enforce_bit_length(weightTable[pc%numEntriesInPerceptronTables][i], 2);
    }

    if (outcome == TAKEN) {
      perceptronTable[pc%numEntriesInPerceptronTables] += 1;
    } 
    else {
      perceptronTable[pc%numEntriesInPerceptronTables] -= 1;
    }
  }
  	GHR = (GHR <<1 | outcome) & ((1<<numEntriesInWeightsVector) - 1);
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void
train_predictor(uint32_t pc, uint8_t outcome)
{
  //
  //TODO: Implement Predictor training
  //
  switch (bpType) {
    case STATIC:
      break;
    case GSHARE:
      train_gshare(pc, outcome);
      break;
    case TOURNAMENT:
      train_tournament(pc, outcome);
      break;
    case CUSTOM:
      train_perceptron(pc, outcome);
      break;
    default:
      break;
  }
}

uint32_t updateChoiceTablePrediction(uint32_t prediction, uint8_t outcome, uint32_t global_prediction) {
      switch(prediction){
        case WN:
          return (outcome==global_prediction)?WT:SN;
          break;
        case SN:
          return (outcome==global_prediction)?WN:SN;
          break;
        case WT:
          return (outcome==global_prediction)?ST:WN;
          break;
        case ST:
          return (outcome==global_prediction)?ST:WT;
          break;
        default:
          return -1;
    }
}

uint32_t updatePrediction(uint32_t prediction, uint8_t outcome){
  switch (prediction) {
    case SN:
      if (outcome == TAKEN){
        return WN;
      }
      else {
        return SN;
      }
      break;
    case WN: 
      if (outcome == TAKEN){
        return WT;
      }
      else {
        return SN;
      }
      break;
    case WT: 
      if (outcome == TAKEN){
        return ST;
      }
      else {
        return WN;
      }
      break;
    case ST:
      if (outcome == TAKEN) {
        return ST;
      }
      else {
        return WT;
      }
    default:
      return -1;
  }
}
