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
uint32_t* globalBHT;
uint32_t numEntriesInGlobalBHT;

// For Tournament
uint32_t* localBHT; // 2 bit predictor
uint32_t numEntriesInLocalBHT;
uint32_t* localPrediction; 
uint32_t numEntriesInLocalPrediction;
uint32_t* choicePredictionTable; 
uint32_t numEntriesInChoiceTable;

//
uint32_t updatePrediction(uint32_t, uint8_t);

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
// FUNCTION: Initializes the Gshare predictor's data structures
void init_gshare() {
  GHR= 0;
  numEntriesInGlobalBHT = pow(2, ghistoryBits) * 2;
  globalBHT = malloc(sizeof(uint32_t)*numEntriesInGlobalBHT);
  for (int i = 0; i < numEntriesInGlobalBHT ; i++){
    globalBHT[i] = WN;
  }
}

// FUNCTION: Initializes the Tournament predictor's data structures
void init_tournament() {
  // Initialize global history components
  init_gshare();

  // Initialize local history components
  numEntriesInLocalBHT = pow(2, lhistoryBits) * 2;
  localBHT = malloc(sizeof(uint32_t)*numEntriesInLocalBHT);
  for (int i = 0; i < numEntriesInLocalBHT; i++){
    localBHT[i] = WN;
  }

  numEntriesInLocalPrediction = pow(2, pcIndexBits) * lhistoryBits;
  localPrediction = malloc(sizeof(uint32_t)*numEntriesInLocalPrediction);
  for (int i = 0; i < numEntriesInLocalPrediction; i++){
    localPrediction[i] = WN;
  }
  // Initialize choice prediction table
  numEntriesInChoiceTable = pow(2, ghistoryBits) * 2;
  choicePredictionTable = malloc(sizeof(uint32_t)*numEntriesInChoiceTable);
  for (int i = 0; i < numEntriesInChoiceTable; i++){
    choicePredictionTable[i] = WN;
  }
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
      break;
    default:
      break;
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
      if (globalBHT[(pc^GHR)%numEntriesInGlobalBHT] > 1){
        return TAKEN;
      }
      else {
        return NOTTAKEN;
      }
    case TOURNAMENT:
      if (choicePredictionTable[GHR%numEntriesInChoiceTable] <= 1){
        // 0 and 1 represent l and L
        return localBHT[(localPrediction[pc%numEntriesInLocalPrediction])%numEntriesInLocalBHT];
      }
      else {
        // 2 and 3 represent g and G
        return globalBHT[GHR%numEntriesInGlobalBHT];
      }
    case CUSTOM:
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// FUNCTION: Train the Gshare predictor
void train_gshare(uint32_t pc, uint8_t outcome){
  uint32_t BHT_index = (pc^GHR)%numEntriesInGlobalBHT;
  globalBHT[BHT_index] = updatePrediction(globalBHT[BHT_index], outcome);
  // Shift global history to add the new outcome
  GHR = ((GHR << 1) + outcome);
}

int determine_taken_nottaken(int curr_prediction) {
  if (curr_prediction <= 1) {
    return NOTTAKEN;
  } else {
    return TAKEN;
  }
}
// FUNCTION: Train the tournament predictor
void train_tournament(uint32_t pc, uint8_t outcome){
  // Calculate the index used for each table
  uint32_t globalBHT_index = GHR%numEntriesInGlobalBHT;
  uint32_t localPrediction_index = pc%numEntriesInLocalPrediction;
  uint32_t localBHT_index = localPrediction[pc%numEntriesInLocalPrediction]%numEntriesInLocalBHT;
  uint32_t choiceTable_index = GHR%numEntriesInChoiceTable;

  // Calculate the local and global prediction values
  uint32_t local_prediction = determine_taken_nottaken(localBHT[localBHT_index]);
  uint32_t global_prediction = determine_taken_nottaken(globalBHT[globalBHT_index]);
  
  // Update global history based on outcome
  globalBHT[globalBHT_index] = updatePrediction(global_prediction, outcome);

  // Update local BHT and local prediction table
  //localPrediction[localPrediction_index] = updatePrediction(localPrediction[localPrediction_index], outcome);
  localBHT[localBHT_index] = updatePrediction(localBHT[localBHT_index], outcome);

  // Update choice table
  if (local_prediction != global_prediction) {
    choicePredictionTable[choiceTable_index] = updatePrediction(choicePredictionTable[choiceTable_index], outcome);
  }

  // Shift global history to add the new outcome
  GHR = ((GHR << 1) + outcome);
  localPrediction[localPrediction_index] = ((localPrediction[localPrediction_index] << 1) + outcome);
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
    case CUSTOM:
    default:
      break;
  }
}

uint32_t updatePrediction(uint32_t prev_value, uint8_t new_value){
  switch (prev_value) {
    case SN:
      if (new_value == TAKEN){
        return WN;
      }
      else {
        return SN;
      }
      break;
    case WN: 
      if (new_value == TAKEN){
        return WT;
      }
      else {
        return SN;
      }
      break;
    case WT: 
      if (new_value == TAKEN){
        return ST;
      }
      else {
        return WN;
      }
      break;
    case ST:
      if (new_value == TAKEN) {
        return ST;
      }
      else {
        return WT;
      }
    default:
      return -1;
  }
}
