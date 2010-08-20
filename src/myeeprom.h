/*
 * Author - Erez Raviv <erezraviv@gmail.com>
 *
 * Based on th9x -> http://code.google.com/p/th9x/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef eeprom_h
#define eeprom_h


//eeprom data
//#define EE_VERSION 2
#define MAX_MODELS 16
#define MAX_MIXERS 32
#define MAX_CURVE5 8
#define MAX_CURVE9 8
#define MDVERS_r9  1
#define MDVERS_r14 2
#define MDVERS_r22 3
#define MDVERS     4


// eeprom ver <9 => mdvers == 1
// eeprom ver >9 => mdvers ==2


typedef struct t_TrainerData1 {
  uint8_t srcChn:3; //0-7 = ch1-8
  int8_t  swtch:5;
  int8_t  studWeight:6;
  uint8_t mode:2;   //off,add-mode,subst-mode
} __attribute__((packed)) TrainerData1; //

typedef struct t_TrainerData {
  int16_t       calib[4];
  TrainerData1  chanMix[4];
} __attribute__((packed)) TrainerData; //

typedef struct t_EEGeneral {
  uint8_t   myVers;
  int16_t   calibMid[4];
  int16_t   calibSpanNeg[4];
  int16_t   calibSpanPos[4];
  uint16_t  chkSum;
  uint8_t   currModel; //0..15
  uint8_t   contrast;
  uint8_t   vBatWarn;
  int8_t    vBatCalib;
  int8_t    lightSw;
  TrainerData trainer;
  uint8_t   view;     //index of subview in main scrren
#define WARN_THR     (!(g_eeGeneral.warnOpts & 0x01))
#define WARN_THR_REV (!(g_eeGeneral.warnOpts & 0x20))
#define WARN_SW      (!(g_eeGeneral.warnOpts & 0x02))
#define WARN_MEM     (!(g_eeGeneral.warnOpts & 0x04))
#define BEEP_VAL     ( (g_eeGeneral.warnOpts & 0x18) >>3 )
  uint8_t   warnOpts; //bitset for several warnings
  uint8_t   stickMode;
  uint8_t   inactivityTimer;
  uint8_t   throttleReversed;
} __attribute__((packed)) EEGeneral;




#define DR_HIGH   0
#define DR_MID    1
#define DR_LOW    2
#define DR_EXPO   0
#define DR_WEIGHT 1
#define DR_RIGHT  0
#define DR_LEFT   1
#define DR_DRSW1  99
#define DR_DRSW2  98

//eeprom modelspec
//expo[3][2][2] //[Norm/Dr][expo/weight][R/L]

typedef struct t_ExpoData {
  int8_t  expo[3][2][2];
  int8_t  drSw1;
  int8_t  drSw2;
} __attribute__((packed)) ExpoData;


typedef struct t_LimitData {
  int8_t  min;
  int8_t  max;
  bool    revert;
  int16_t  offset;
} __attribute__((packed)) LimitData;

#define MLTPX_ADD  0
#define MLTPX_MUL  1
#define MLTPX_REP  2

typedef struct t_MixData {
  uint8_t destCh;            //        1..NUM_CHNOUT
  uint8_t srcRaw;            //
  int8_t  weight;
  int8_t  swtch;
  uint8_t curve;             //0=symmetrisch 1=no neg 2=no pos
  uint8_t delayUp:4;
  uint8_t delayDown:4;
  uint8_t speedUp:4;         // Servogeschwindigkeit aus Tabelle (10ms Cycle)
  uint8_t speedDown:4;       // 0 nichts
  uint8_t carryTrim:1;
  uint8_t mltpx:3;           // multiplex method 0=+ 1=* 2=replace
  uint8_t boolres:4;
} __attribute__((packed)) MixData;


typedef struct t_ModelData {
  char      name[10];             // 10 must be first for eeLoadModelName
  uint8_t   mdVers;
  uint8_t   tmrMode;
  uint16_t  tmrVal;
  uint8_t   protocol;
  int8_t    ppmNCH;
  int8_t    thrTrim:4;            // Enable Throttle Trim
  int8_t    thrExpo:4;            // Enable Throttle Expo
  int8_t    trimInc;              // Trim Increments
  //int8_t    tcutSW;               // Throttle cut switch
  int8_t    ppmDelay;
  //int8_t    tcutTarget;
  char      res[5];
  MixData   mixData[MAX_MIXERS];
  LimitData limitData[NUM_CHNOUT];
  ExpoData  expoData[4];
  int8_t    trim[4];
  int8_t    curves5[MAX_CURVE5][5];
  int8_t    curves9[MAX_CURVE9][9];
} __attribute__((packed)) ModelData;



#define TOTAL_EEPROM_USAGE (sizeof(ModelData)*MAX_MODELS + sizeof(EEGeneral))

//===================================================
// Previous versions
//===================================================
// r22 - mdvers == 3
typedef struct t_MixData_r22 {
  uint8_t destCh; //        1..NUM_CHNOUT,X1-X4
  uint8_t srcRaw:7; //0=off   1..8      ,X1-X4
  uint8_t carryTrim:1;
  int8_t  weight;
  int8_t  swtch:6;
  uint8_t curve:5;           //0=symmetrisch 1=no neg 2=no pos
  uint8_t startDelay:5;
  uint8_t speedUp:4;         // Servogeschwindigkeit aus Tabelle (10ms Cycle)
  uint8_t speedDown:4;      // 0 nichts
} __attribute__((packed)) MixData_r22;


typedef struct t_ModelData_r22 {
  char      name[10];             // 10 must be first for eeLoadModelName
  uint8_t   mdVers;
  uint8_t   tmrMode;
  uint16_t  tmrVal;
  uint8_t   protocol;
  int8_t    ppmNCH;
  int8_t    thrTrim:4;            // Enable Throttle Trim
  int8_t    thrExpo:4;            // Enable Throttle Expo
  int8_t    trimInc;              // Trim Increments
  int8_t    tcutSW;               // Throttle cut switch
  int8_t    ppmDelay;
  int8_t    tcutTarget;
  char      res[3];
  MixData_r22   mixData[25];
  LimitData limitData[16];
  ExpoData  expoData[4];
  int8_t    trim[4];
  int8_t    curves5[8][5];
  int8_t    curves9[8][9];
} __attribute__((packed)) ModelData_r22;

// r14 - mdvers == 2
typedef struct t_ExpoData_r14 {
  int8_t  expNormR;
  int8_t  expNormL;
  int8_t  expDrR;
  int8_t  expDrL;
  int8_t  drSw;
  int8_t  expNormWeightR;
  int8_t  expNormWeightL;
  int8_t  expSwWeightR;
  int8_t  expSwWeightL;
} __attribute__((packed)) ExpoData_r14;

typedef struct t_ModelData_r14 {
  char      name[10];             // 10 must be first for eeLoadModelName
  uint8_t   mdVers;               // 1
  uint8_t   tmrMode;              // 1
  uint16_t  tmrVal;               // 2
  uint8_t   protocol;             // 1
  uint8_t   ppmNCH;               // 1
  int8_t    thrTrim;            // 1 Enable Trottle Trim
  int8_t    trimInc;            // Trim Increments
  int8_t    tcutSW;             // Throttle cut switch
  char      res[5];               // 5
  MixData   mixData[25];  //0 4*25
  LimitData limitData[16];// 4*8
  ExpoData_r14  expoData[4];          // 5*4
  int8_t    trim[4];          // 3*4
  int8_t    curves5[8][5];        // 10
  int8_t    curves9[8][9];        // 18
} __attribute__((packed)) ModelData_r14; //211



// r9 - mdvers == 1
typedef struct t_ExpoData_r9 {
  int8_t  expNorm;
  int8_t  expDr;
  int8_t  drSw;
  int8_t  expNormWeight;
  int8_t  expSwWeight;
} __attribute__((packed)) ExpoData_r9;

typedef struct t_TrimData_r9 {
  int8_t  trim;    //quadratisch
  int16_t trimDef;
} __attribute__((packed)) TrimData_r9;


typedef struct t_ModelData_r9 {
  char      name[10];             // 10 must be first for eeLoadModelName
  uint8_t   mdVers;               // 1
  uint8_t   tmrMode;              // 1
  uint16_t  tmrVal;               // 2
  uint8_t   protocol;             // 1
  uint8_t   ppmNCH;               // 1
  char      res[2];               // 2
  int8_t    thrTrim;            // 1 Enable Trottle Trim
  int8_t    trimInc;            // Trim Increments
  int8_t    tcutSW;             // Throttle cut switch
  MixData   mixData[25];  //0 4*25
  TrimData_r9  trimData[4];          // 3*4
  LimitData limitData[16];// 4*8
  ExpoData_r9  expoData[4];          // 5*4
  int8_t    curves5[8][5];        // 10
  int8_t    curves9[8][9];        // 18

} __attribute__((packed)) ModelData_r9; //211




extern EEGeneral g_eeGeneral;
extern ModelData g_model;












#endif
/*eof*/
