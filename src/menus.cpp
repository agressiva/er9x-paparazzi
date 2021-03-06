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

#include "er9x.h"
#include "templates.h"

#define NUM_OFS(x) (((x<0 ? 2*FW-1 : 1*FW) + ((abs(x)>=100) ? 2*FW-2 : ((abs(x)>=10) ? 1*FW-1 : 0 ))))

#define GET_DR_STATE(x) (!getSwitch(g_model.expoData[x].drSw1,0) ?   \
                          DR_HIGH :                                  \
                          !getSwitch(g_model.expoData[x].drSw2,0)?   \
                          DR_MID : DR_LOW);

//#define DO_SQUARE(xx,yy,ww)
//    lcd_vline(xx-ww/2,yy-ww/2,ww);
//    lcd_hline(xx-ww/2,yy+ww/2,ww);
//    lcd_vline(xx+ww/2,yy-ww/2,ww);
//    lcd_hline(xx-ww/2,yy-ww/2,ww);

#define DO_SQUARE(xx,yy,ww)         \
    {uint8_t x,y,w ; x = xx; y = yy; w = ww ; \
    lcd_vline(x-w/2,y-w/2,w);  \
    lcd_hline(x-w/2,y+w/2,w);  \
    lcd_vline(x+w/2,y-w/2,w);  \
    lcd_hline(x-w/2,y-w/2,w);}

#define DO_CROSS(xx,yy,ww)          \
    lcd_vline(xx,yy-ww/2,ww);  \
    lcd_hline(xx-ww/2,yy,ww);  \

#define V_BAR(xx,yy,ll)       \
    lcd_vline(xx-1,yy-ll,ll); \
    lcd_vline(xx  ,yy-ll,ll); \
    lcd_vline(xx+1,yy-ll,ll); \

#define NO_HI_LEN 25

#define WCHART 32
#define X0     (128-WCHART-2)
#define Y0     32
#define WCHARTl 32l
#define X0l     (128l-WCHARTl-2)
#define Y0l     32l
#define RESX    (1<<10) // 1024
#define RESXu   1024u
#define RESXul  1024ul
#define RESXl   1024l
#define RESKul  100ul
#define RESX_PLUS_TRIM (RESX+128)

int16_t calibratedStick[7];
int16_t ex_chans[NUM_CHNOUT];          // Outputs + intermidiates
uint8_t s_pgOfs;
uint8_t s_editMode;
uint8_t s_noHi;

int16_t g_chans512[NUM_CHNOUT];

extern bool warble;
extern int16_t p1valdiff;

extern MixData *mixaddress( uint8_t idx ) ;

#include "sticks.lbm"
typedef PROGMEM void (*MenuFuncP_PROGMEM)(uint8_t event);

MenuFuncP_PROGMEM APM menuTabModel[] = {
  menuProcModelSelect,
  menuProcModel,
  menuProcHeli,
  menuProcExpoAll,
  menuProcMix,
  menuProcLimits,
  menuProcCurve,
  menuProcSwitches,
  menuProcSafetySwitches,
  menuProcTemplates
};

MenuFuncP_PROGMEM APM menuTabDiag[] = {
  menuProcSetup,
  menuProcTrainer,
  menuProcDiagVers,
  menuProcDiagKeys,
  menuProcDiagAna,
  menuProcDiagCalib
};


//#define PARR8(args...) (__extension__({static prog_uint8_t APM __c[] = args;&__c[0];}))
struct MState2
{
  uint8_t m_posVert;
  uint8_t m_posHorz;
  void init(){m_posVert=m_posHorz=0;};
  prog_uint8_t *m_tab;
  static uint8_t event;
  void check_v(uint8_t event,  uint8_t curr,MenuFuncP *menuTab, uint8_t menuTabSize, uint8_t maxrow);
  void check(uint8_t event,  uint8_t curr,MenuFuncP *menuTab, uint8_t menuTabSize, prog_uint8_t*subTab,uint8_t subTabMax,uint8_t maxrow);
};
#define MSTATE_TAB  static prog_uint8_t APM mstate_tab[]
#define MSTATE_CHECK0_VxH(numRows) mstate2.check(event,0,0,0,mstate_tab,DIM(mstate_tab)-1,numRows-1)
#define MSTATE_CHECK0_V(numRows) mstate2.check_v(event,0,0,0,numRows-1)
#define MSTATE_CHECK_VxH(curr,menuTab,numRows) mstate2.check(event,curr,menuTab,DIM(menuTab),mstate_tab,DIM(mstate_tab)-1,numRows-1)
#define MSTATE_CHECK_V(curr,menuTab,numRows) mstate2.check_v(event,curr,menuTab,DIM(menuTab),numRows-1)


void MState2::check_v(uint8_t event,  uint8_t curr,MenuFuncP *menuTab, uint8_t menuTabSize, uint8_t maxrow)
{
  check( event,  curr, menuTab, menuTabSize, 0, 0, maxrow);
}
void MState2::check(uint8_t event,  uint8_t curr,MenuFuncP *menuTab, uint8_t menuTabSize, prog_uint8_t*horTab,uint8_t horTabMax,uint8_t maxrow)
{
  if(menuTab){
    uint8_t attr = m_posVert==0 ? INVERS : 0;
    curr--; //calc from 0, user counts from 1

    if(m_posVert==0){
      switch(event)
      {
        case EVT_KEY_FIRST(KEY_LEFT):
          if(curr>0){
            chainMenu((MenuFuncP)pgm_read_adr(&menuTab[curr-1]));
          }
          break;
        case EVT_KEY_FIRST(KEY_RIGHT):
          if(curr < (menuTabSize-1)){
            chainMenu((MenuFuncP)pgm_read_adr(&menuTab[curr+1]));
          }
          break;
      }
    }
    lcd_outdezAtt(128,0,menuTabSize,attr);
    lcd_putcAtt(1+128-FW*(menuTabSize>9 ? 3 : 2),0,'/',attr);
    lcd_outdezAtt(1+128-FW*(menuTabSize>9 ? 3 : 2),0,curr+1,attr);
  }

#define MAXCOL(row) (horTab ? pgm_read_byte(horTab+min( row, horTabMax ))-1 : 0)
#define INC(val,max) if(val<max) {val++;} else {val=0;}
#define DEC(val,max) if(val>0  ) {val--;} else {val=max;}
  uint8_t maxcol = MAXCOL(m_posVert);
  switch(event)
  {
    case EVT_ENTRY:
      //if(m_posVert>maxrow)
        m_posVert=0;
      //init();BLINK_SYNC;
      break;
    case EVT_KEY_LONG(KEY_EXIT):
      s_editMode = false;
      //popMenu(true); //return to uppermost, beeps itself
      popMenu(false);
      break;
      //fallthrough
    case EVT_KEY_BREAK(KEY_EXIT):
      if(s_editMode) {
        s_editMode = false;
        break;
      }
      if(m_posVert==0 || !menuTab) {
        popMenu();  //beeps itself
      } else {
        beepKey();
        init();BLINK_SYNC;
      }
      break;

    case EVT_KEY_REPT(KEY_RIGHT):  //inc
      if(m_posHorz==maxcol) break;
    case EVT_KEY_FIRST(KEY_RIGHT)://inc
      if(!horTab || s_editMode)break;
      INC(m_posHorz,maxcol);
      BLINK_SYNC;
      break;

    case EVT_KEY_REPT(KEY_LEFT):  //dec
      if(m_posHorz==0) break;
    case EVT_KEY_FIRST(KEY_LEFT)://dec
      if(!horTab || s_editMode)break;
      DEC(m_posHorz,maxcol);
      BLINK_SYNC;
      break;

    case EVT_KEY_REPT(KEY_DOWN):  //inc
      if(m_posVert==maxrow) break;
    case EVT_KEY_FIRST(KEY_DOWN): //inc
      if(s_editMode)break;
      INC(m_posVert,maxrow);
      BLINK_SYNC;
      break;

    case EVT_KEY_REPT(KEY_UP):  //dec
      if(m_posVert==0) break;
    case EVT_KEY_FIRST(KEY_UP): //dec
      if(s_editMode)break;
      DEC(m_posVert,maxrow);
      BLINK_SYNC;
      break;

  }
}


#define TITLEP(pstr) lcd_putsAtt(0,0,pstr,INVERS)
#define TITLE(str)   TITLEP(PSTR(str))

static uint8_t s_curveChan;

#define XD X0-2

void menuProcCurveOne(uint8_t event) {
  static MState2 mstate2;
  uint8_t x = TITLE("CURVE ");
  lcd_outdezAtt(x, 0,s_curveChan+1 ,INVERS);

  bool    cv9 = s_curveChan >= MAX_CURVE5;
  //MSTATE_CHECK0_V((cv9 ? 9 : 5)+1);
  MSTATE_TAB = { 1+9,1,1,1,1,1,1,1,1,1,1,1 };
  MSTATE_CHECK0_VxH((cv9 ? 9 : 5)+2);
  int8_t *crv = cv9 ? g_model.curves9[s_curveChan-MAX_CURVE5] : g_model.curves5[s_curveChan];

  int8_t  sub    = mstate2.m_posVert-1;
  int8_t  subSub = mstate2.m_posHorz;

  switch(event){
    case EVT_KEY_FIRST(KEY_EXIT):
      if(sub!=-1 || subSub!=0) killEvents(event);
    case EVT_ENTRY:
      s_editMode = false;
      mstate2.m_posVert = 0;
      mstate2.m_posHorz = 0;
      break;

    case EVT_KEY_REPT(KEY_LEFT):
    case EVT_KEY_FIRST(KEY_LEFT):
      if(s_editMode && mstate2.m_posHorz>0) mstate2.m_posHorz--;
      break;
    case EVT_KEY_REPT(KEY_RIGHT):
    case EVT_KEY_FIRST(KEY_RIGHT):
      if((s_editMode) && (subSub<(cv9 ? 9 : 5))) mstate2.m_posHorz++;
      break;
  }
  s_editMode = subSub;
  if(s_editMode) {
      mstate2.m_posVert = 0;
  }

  if(mstate2.m_posHorz>=(cv9 ? 9 : 5)) mstate2.m_posHorz=(cv9 ? 9 : 5);
  if(mstate2.m_posHorz<0) mstate2.m_posHorz=0;


  for (uint8_t i = 0; i < 5; i++) {
    uint8_t y = i * FH + 16;
    uint8_t attr = sub == i ? INVERS : 0;
    lcd_outdezAtt(4 * FW, y, crv[i], attr);
  }
  if(cv9)
    for (uint8_t i = 0; i < 4; i++) {
      uint8_t y = i * FH + 16;
      uint8_t attr = sub == i + 5 ? INVERS : 0;
      lcd_outdezAtt(8 * FW, y, crv[i + 5], attr);
    }
  lcd_putsAtt( 2*FW, 1*FH,PSTR("EDIT->"),((sub == -1) && (subSub == 0)) ? INVERS : 0);
  lcd_putsAtt( 2*FW, 7*FH,PSTR("PRESET"),sub == (cv9 ? 9 : 5) ? INVERS : 0);

  static int8_t dfltCrv;
  if((sub<(cv9 ? 9 : 5)) && (sub>-1))  CHECK_INCDEC_H_MODELVAR( event, crv[sub], -100,100);
  else  if(sub>0){ //make sure we're not on "EDIT"
    if( checkIncDecGen2(event, &dfltCrv, -4, 4, 0)){
      if(cv9) for (uint8_t i = 0; i < 9; i++) crv[i] = (i-4)*dfltCrv* 100 / 16;
      else    for (uint8_t i = 0; i < 5; i++) crv[i] = (i-2)*dfltCrv* 100 /  8;
      eeDirty(EE_MODEL);
    }
  }

  if(s_editMode)
  {
    for(uint8_t i=0; i<(cv9 ? 9 : 5); i++)
    {
      uint8_t xx = XD-1-WCHART+i*WCHART/(cv9 ? 4 : 2);
      uint8_t yy = Y0-crv[i]*WCHART/100;


      if(subSub==(i+1))
      {
        if((yy-2)<WCHART*2) lcd_hline( xx-1, yy-2, 5); //do selection square
        if((yy-1)<WCHART*2) lcd_hline( xx-1, yy-1, 5);
        if(yy<WCHART*2)     lcd_hline( xx-1, yy  , 5);
        if((yy+1)<WCHART*2) lcd_hline( xx-1, yy+1, 5);
        if((yy+2)<WCHART*2) lcd_hline( xx-1, yy+2, 5);

        if(p1valdiff || event==EVT_KEY_FIRST(KEY_DOWN) || event==EVT_KEY_FIRST(KEY_UP) || event==EVT_KEY_REPT(KEY_DOWN) || event==EVT_KEY_REPT(KEY_UP))
           CHECK_INCDEC_H_MODELVAR( event, crv[i], -100,100);  // edit on up/down
      }
      else
      {
          if((yy-1)<WCHART*2) lcd_hline( xx, yy-1, 3); // do markup square
          if(yy<WCHART*2)     lcd_hline( xx, yy  , 3);
          if((yy+1)<WCHART*2) lcd_hline( xx, yy+1, 3);
      }
    }
  }

  for (uint8_t xv = 0; xv < WCHART * 2; xv++) {
    uint16_t yv = intpol(xv * (RESXu / WCHART) - RESXu, s_curveChan) / (RESXu
                                                                      / WCHART);
    lcd_plot(XD + xv - WCHART, Y0 - yv);
    if ((xv & 3) == 0) {
      lcd_plot(XD + xv - WCHART, Y0 + 0);
    }
  }
  lcd_vline(XD, Y0 - WCHART, WCHART * 2);
}



void menuProcCurve(uint8_t event) {
  static MState2 mstate2;
  TITLE("CURVE");
  MSTATE_CHECK_V(7,menuTabModel,MAX_CURVE5+MAX_CURVE9+1);
  int8_t  sub    = mstate2.m_posVert - 1;

  if(sub<1) s_pgOfs=0;
  else if((sub-s_pgOfs)>6) s_pgOfs = sub-6;
  else if((sub-s_pgOfs)<0) s_pgOfs = sub;
  if(s_pgOfs<0) s_pgOfs = 0;

  switch (event) {
    case EVT_ENTRY:
      s_pgOfs = 0;
      break;
    case EVT_KEY_FIRST(KEY_RIGHT):
    case EVT_KEY_FIRST(KEY_MENU):
      if (sub >= 0) {
        s_curveChan = sub;
        pushMenu(menuProcCurveOne);
      }
      break;
  }

  uint8_t y    = 1*FH;
  uint8_t yd   = 1;
  uint8_t m    = 0;
  for (uint8_t i = 0; i < 7; i++) {
    uint8_t k = i + s_pgOfs;
    uint8_t attr = sub == k ? INVERS : 0;
    bool    cv9 = k >= MAX_CURVE5;

    if(cv9 && (yd>6)) break;
    if(yd>7) break;
    if(!m) m = attr;
    lcd_putsAtt(   FW*0, y,PSTR("CV"),attr);
    lcd_outdezAtt( (k<9) ? FW*3 : FW*4-1, y,k+1 ,attr);

    int8_t *crv = cv9 ? g_model.curves9[k-MAX_CURVE5] : g_model.curves5[k];
    for (uint8_t j = 0; j < (5); j++) {
      lcd_outdezAtt( j*(3*FW+3) + 7*FW, y, crv[j], 0);
    }
    y += FH;yd++;
    if(cv9){
      for (uint8_t j = 0; j < 4; j++) {
        lcd_outdezAtt( j*(3*FW+3) + 7*FW, y, crv[j+5], 0);
      }
      y += FH;yd++;
    }
  }

  if(!m) s_pgOfs++;
}



static bool  s_limitCacheOk;
#define LIMITS_DIRTY s_limitCacheOk=false

void setStickCenter() // copy state of 3 primary to subtrim
{
      int16_t zero_chans512[NUM_CHNOUT];
      perOut(zero_chans512,true); // do output loop - zero input channels

      for(uint8_t i=0; i<NUM_CHNOUT; i++)
      {
          int16_t v = g_model.limitData[i].offset;
          v += g_model.limitData[i].revert ?
               (zero_chans512[i] - g_chans512[i]) :
               -(zero_chans512[i] - g_chans512[i]);
          g_model.limitData[i].offset = max(min(v,1000),-1000); // make sure the offset doesn't go haywire
      }

      for(uint8_t i=0; i<4; i++)
        if(!IS_THROTTLE(i)) g_model.trim[i] = 0;// set trims to zero.
      STORE_MODELVARS;
      beepWarn1();
}

void menuProcLimits(uint8_t event)
{
  static MState2 mstate2;
  static bool swVal[NUM_CHNOUT];
  TITLE("LIMITS");
  MSTATE_TAB = { 1,4};
  MSTATE_CHECK_VxH(6,menuTabModel,NUM_CHNOUT+2);

  uint8_t y = 0;
  uint8_t k = 0;
  int8_t  sub    = mstate2.m_posVert - 1;
  uint8_t subSub = mstate2.m_posHorz + 1;
  if(sub<1) s_pgOfs=0;
  else if((sub-s_pgOfs)>6) s_pgOfs = sub-6;
  else if((sub-s_pgOfs)<0) s_pgOfs = sub;
  if(s_pgOfs<0) s_pgOfs = 0;

  switch(event)
  {
    case EVT_ENTRY:
      s_pgOfs = 0;
      s_editMode = false;
      break;
    case EVT_KEY_FIRST(KEY_MENU):
      if((sub>=0) && (subSub!=4)) s_editMode = !s_editMode;
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      if(s_editMode) {
        s_editMode = false;
        killEvents(event);
      }
      break;
    case EVT_KEY_LONG(KEY_MENU):
      if(sub>=0 && sub<NUM_CHNOUT) {
          int16_t v = g_chans512[sub - s_pgOfs];
          LimitData *ld = &g_model.limitData[sub];
          switch (subSub) {
          case 1:
              ld->offset = (ld->revert) ? -v : v;
              STORE_MODELVARS;
              break;
          }
      }
      break;
  }
//  lcd_puts_P( 4*FW, 1*FH,PSTR("subT min  max inv"));
  for(uint8_t i=0; i<7; i++){
    y=(i+1)*FH;
    k=i+s_pgOfs;
    if(k==NUM_CHNOUT) break;
    LimitData *ld = &g_model.limitData[k];
    for(uint8_t j=0; j<=4;j++){
      uint8_t attr = ((sub==k && subSub==j) ? (s_editMode ? BLINK : INVERS) : 0);
      int16_t v = (ld->revert) ? -ld->offset : ld->offset;
      if((g_chans512[k] - v) >  50) swVal[k] = (true==ld->revert);// Switch to raw inputs?  - remove trim!
      if((g_chans512[k] - v) < -50) swVal[k] = (false==ld->revert);
      lcd_putcAtt(12*FW+FW/2, y, (swVal[k] ? 127 : 126),0); //'<' : '>'
      switch(j)
      {
        case 0:
          putsChn(0,y,k+1,(sub==k && subSub==0) ? INVERS : 0);
          break;
        case 1:
          lcd_outdezAtt(  8*FW, y,  ld->offset, attr|PREC1);
          if(attr && (s_editMode || p1valdiff)) {
            if(CHECK_INCDEC_H_MODELVAR( event, ld->offset, -1000,1000))  LIMITS_DIRTY;
          }
          break;
        case 2:
          lcd_outdezAtt(  12*FW, y, (int8_t)(ld->min-100),   attr);
          if(attr && (s_editMode || p1valdiff)) {
              ld->min -=  100;
              if(g_model.extendedLimits)
              {if(CHECK_INCDEC_H_MODELVAR( event, ld->min, -125,125))  LIMITS_DIRTY;}
              else
              {if(CHECK_INCDEC_H_MODELVAR( event, ld->min, -100,100))  LIMITS_DIRTY;}
              ld->min +=  100;
          }
          break;
        case 3:
          lcd_outdezAtt( 17*FW, y, (int8_t)(ld->max+100),    attr);
          if(attr && (s_editMode || p1valdiff)) {
              ld->max +=  100;
              if(g_model.extendedLimits)
              {if(CHECK_INCDEC_H_MODELVAR( event, ld->max, -125,125))  LIMITS_DIRTY;}
              else
              {if(CHECK_INCDEC_H_MODELVAR( event, ld->max, -100,100))  LIMITS_DIRTY;}
              ld->max -=  100;
          }
          break;
        case 4:
          lcd_putsnAtt(   18*FW, y, PSTR("---INV")+ld->revert*3,3,attr);
          //if(attr && (s_editMode || p1valdiff)) {
          if(attr && (event==EVT_KEY_FIRST(KEY_MENU))) {
            //CHECK_INCDEC_H_MODELVAR_BF( event, ld->revert,    0,1);
            ld->revert = !(ld->revert);
            STORE_MODELVARS;
          }
          break;
      }
    }
  }
  if(k==NUM_CHNOUT){
    //last line available - add the "copy trim menu" line
    uint8_t attr = (sub==NUM_CHNOUT) ? INVERS : 0;
    lcd_putsAtt(  3*FW,y,PSTR("COPY TRIM [MENU]"),s_noHi ? 0 : attr);
    if(attr && event==EVT_KEY_LONG(KEY_MENU)) {
      s_editMode = false;
      s_noHi = NO_HI_LEN;
      killEvents(event);
      setStickCenter(); //if highlighted and menu pressed - copy trims
    }
  }
}

void menuProcTemplates(uint8_t event)  //Issue 73
{
  static MState2 mstate2;
  TITLE("TEMPLATES");
  MSTATE_CHECK_V(10,menuTabModel,NUM_TEMPLATES+3);

  uint8_t y = 0;
  uint8_t k = 0;
  int8_t  sub    = mstate2.m_posVert - 1;
  if(sub<1) s_pgOfs=0;
  else if((sub-s_pgOfs)>6) s_pgOfs = sub-6;
  else if((sub-s_pgOfs)<0) s_pgOfs = sub;
  if(s_pgOfs<0) s_pgOfs = 0;

  switch(event)
  {
    case EVT_ENTRY:
      s_pgOfs = 0;
      break;
    case EVT_KEY_LONG(KEY_MENU):
      killEvents(event);
      //apply mixes or delete
      s_noHi = NO_HI_LEN;
      if(sub==NUM_TEMPLATES+1)
        clearMixes();
      else if((sub>=0) && (sub<(int8_t)NUM_TEMPLATES))
        applyTemplate(sub);
      beepWarn1();
      break;
  }

  y=1*FH;
  for(uint8_t i=0; i<7; i++){
    k=i+s_pgOfs;
    if(k==NUM_TEMPLATES) break;

    //write mix names here
    lcd_outdezNAtt(3*FW, y, k+1, (sub==k ? INVERS : 0) + LEADING0,2);
    lcd_putsAtt(  4*FW, y, n_Templates[k],BSS_NO_INV | (s_noHi ? 0 : (sub==k ? INVERS  : 0)));
    y+=FH;
  }

  if(y>7*FH) return;
  uint8_t attr = s_noHi ? 0 : ((sub==NUM_TEMPLATES) ? INVERS : 0);
  lcd_puts_P( 1*FW, y,PSTR("Channel Order"));//   RAET->AETR

//  lcd_putsnAtt(15*FW, y, PSTR(" RETA")+CHANNEL_ORDER(1),1,attr);
//  lcd_putsnAtt(16*FW, y, PSTR(" RETA")+CHANNEL_ORDER(2),1,attr);
//  lcd_putsnAtt(17*FW, y, PSTR(" RETA")+CHANNEL_ORDER(3),1,attr);
//  lcd_putsnAtt(18*FW, y, PSTR(" RETA")+CHANNEL_ORDER(4),1,attr);

	{
		uint8_t i ;
		for ( i = 1 ; i <= 4 ; i += 1 )
		{
  		lcd_putsnAtt((14+i)*FW, y, PSTR(" RETA")+CHANNEL_ORDER(i),1,attr);
		}
	}


  if(attr) CHECK_INCDEC_H_GENVAR(event, g_eeGeneral.templateSetup, 0, 23);
  y+=FH;

  if(y>7*FH) return;
  attr = s_noHi ? 0 : ((sub==NUM_TEMPLATES+1) ? INVERS : 0);
  lcd_putsAtt(  1*FW,y,PSTR("CLEAR MIXES [MENU]"),attr);
  y+=FH;

}

void menuProcSafetySwitches(uint8_t event)
{
  static MState2 mstate2;
  TITLE("SAFETY SWITCHES");
  MSTATE_TAB = { 1,2};
  MSTATE_CHECK_VxH(9,menuTabModel,NUM_CHNOUT+1);

  uint8_t y = 0;
  uint8_t k = 0;
  int8_t  sub    = mstate2.m_posVert - 1;
  uint8_t subSub = mstate2.m_posHorz + 1;
  if(sub<1) s_pgOfs=0;
  else if((sub-s_pgOfs)>6) s_pgOfs = sub-6;
  else if((sub-s_pgOfs)<0) s_pgOfs = sub;
  if(s_pgOfs<0) s_pgOfs = 0;

  switch(event)
  {
    case EVT_ENTRY:
      s_pgOfs = 0;
      s_editMode = false;
      break;
    case EVT_KEY_FIRST(KEY_MENU):
      if((sub>=0) && (subSub!=4)) s_editMode = !s_editMode;
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      if(s_editMode) {
        s_editMode = false;
        killEvents(event);
      }
      break;
  }

//  lcd_puts_P( 0*FW, 1*FH,PSTR("ch    sw     val"));
  for(uint8_t i=0; i<7; i++){
    y=(i+1)*FH;
    k=i+s_pgOfs;
    if(k==NUM_CHNOUT) break;

    SafetySwData *sd = &g_model.safetySw[k];
    for(uint8_t j=0; j<=2;j++){
      uint8_t attr = ((sub==k && subSub==j) ? (s_editMode ? BLINK : INVERS) : 0);

      switch(j)
      {
      case 0:
          putsChn(0,y,k+1,(sub==k && subSub==0) ? INVERS : 0);
          break;
      case 1:
          putsDrSwitches(5*FW, y, sd->swtch  , attr);
          if(attr && (s_editMode || p1valdiff)) {
              CHECK_INCDEC_H_MODELVAR( event, sd->swtch, -MAX_DRSWITCH,MAX_DRSWITCH);
          }
          break;
      case 2:
          lcd_outdezAtt(  16*FW, y, sd->val,   attr);
          if(attr && (s_editMode || p1valdiff)) {
              CHECK_INCDEC_H_MODELVAR( event, sd->val, -125,125);
          }
          break;
      }
    }
  }

}

void menuProcSwitches(uint8_t event)  //Issue 78
{
  static MState2 mstate2;
  TITLE("CSWITCH");
  MSTATE_TAB = { 3,3};
  MSTATE_CHECK_VxH(8,menuTabModel,NUM_CSW+1);

  uint8_t y = 0;
  uint8_t k = 0;
  int8_t  sub    = mstate2.m_posVert - 1;
  uint8_t subSub = mstate2.m_posHorz + 1;

  if(sub<1) s_pgOfs=0;
  else if((sub-s_pgOfs)>6) s_pgOfs = sub-6;
  else if((sub-s_pgOfs)<0) s_pgOfs = sub;
  if(s_pgOfs<0) s_pgOfs = 0;

  switch(event)
  {
    case EVT_ENTRY:
      s_pgOfs = 0;
      s_editMode = false;
      break;
    case EVT_KEY_FIRST(KEY_MENU):
      if(sub>=0) s_editMode = !s_editMode;
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      if(s_editMode) {
        s_editMode = false;
        killEvents(event);
      }
      break;
  }

//  lcd_puts_P( 4*FW, 1*FH,PSTR("Function V1  V2"));
  for(uint8_t i=0; i<7; i++){
    y=(i+1)*FH;
    k=i+s_pgOfs;
    if(k==NUM_CSW) break;
    uint8_t attr = (sub==k ? (s_editMode ? BLINK : INVERS)  : 0);
    CSwData &cs = g_model.customSw[k];

    //write SW names here
    lcd_putsnAtt( 0*FW , y, PSTR("SW"),2,0);
    lcd_putcAtt(  2*FW , y, k + (k>8 ? 'A'-9: '1'),0);
    lcd_putsnAtt( 4*FW , y, PSTR(CSWITCH_STR)+CSW_LEN_FUNC*cs.func,CSW_LEN_FUNC,subSub==1 ? attr : 0);

    uint8_t cstate = CS_STATE(cs.func);

    if(cstate == CS_VOFS)
    {
        putsChnRaw(    12*FW, y, cs.v1  ,subSub==2 ? attr : 0);
        lcd_outdezAtt( 20*FW, y, cs.v2  ,subSub==3 ? attr : 0);
    }
    else if(cstate == CS_VBOOL)
    {
        putsDrSwitches(12*FW, y, cs.v1  ,subSub==2 ? attr : 0);
        putsDrSwitches(16*FW, y, cs.v2  ,subSub==3 ? attr : 0);
    }
    else // cstate == CS_COMP
    {
        putsChnRaw(    12*FW, y, cs.v1  ,subSub==2 ? attr : 0);
        putsChnRaw(    17*FW, y, cs.v2  ,subSub==3 ? attr : 0);
    }

    if((s_editMode || p1valdiff) && attr)
      switch (subSub) {
        case (1):
          CHECK_INCDEC_H_MODELVAR( event, cs.func, 0,CS_MAXF);
          if(cstate != CS_STATE(cs.func))
          {
              cs.v1  = 0;
              cs.v2 = 0;
          }
          break;
        case (2):
          switch (cstate) {
          case (CS_VOFS):
              CHECK_INCDEC_H_MODELVAR( event, cs.v1, 0,NUM_XCHNRAW);
              break;
          case (CS_VBOOL):
              CHECK_INCDEC_H_MODELVAR( event, cs.v1, -MAX_DRSWITCH,MAX_DRSWITCH);
              break;
          case (CS_VCOMP):
              CHECK_INCDEC_H_MODELVAR( event, cs.v1, 0,NUM_XCHNRAW);
              break;
          default:
              break;
          }
          break;
        case (3):
          switch (cstate) {
          case (CS_VOFS):
              CHECK_INCDEC_H_MODELVAR( event, cs.v2, -125,125);
              break;
          case (CS_VBOOL):
              CHECK_INCDEC_H_MODELVAR( event, cs.v2, -MAX_DRSWITCH,MAX_DRSWITCH);
              break;
          case (CS_VCOMP):
              CHECK_INCDEC_H_MODELVAR( event, cs.v2, 0,NUM_XCHNRAW);
              break;
          default:
              break;
          }
      }
  }
}

static int8_t s_currMixIdx;
static int8_t s_currDestCh;
static bool   s_currMixInsMode;


void deleteMix(uint8_t idx)
{
  memmove(&g_model.mixData[idx],&g_model.mixData[idx+1],
            (MAX_MIXERS-(idx+1))*sizeof(MixData));
  memset(&g_model.mixData[MAX_MIXERS-1],0,sizeof(MixData));
  STORE_MODELVARS;
}

void insertMix(uint8_t idx)
{
  memmove(&g_model.mixData[idx+1],&g_model.mixData[idx],
         (MAX_MIXERS-(idx+1))*sizeof(MixData) );
  memset(&g_model.mixData[idx],0,sizeof(MixData));
  g_model.mixData[idx].destCh      = s_currDestCh; //-s_mixTab[sub];
  g_model.mixData[idx].srcRaw      = s_currDestCh; //1;   //
  g_model.mixData[idx].weight      = 100;
  STORE_MODELVARS;
}

void menuProcMixOne(uint8_t event)
{
  static MState2 mstate2;
  uint8_t x=TITLEP(s_currMixInsMode ? PSTR("INSERT MIX ") : PSTR("EDIT MIX "));
  MixData *md2 = mixaddress( s_currMixIdx ) ;
  putsChn(x+1*FW,0,md2->destCh,0);
  MSTATE_CHECK0_V(13);
  int8_t  sub    = mstate2.m_posVert;

  if(sub<1) s_pgOfs=0;
  else if((sub-s_pgOfs)>6) s_pgOfs = sub-6;
  else if((sub-s_pgOfs)<0) s_pgOfs = sub;
  if(s_pgOfs<0) s_pgOfs = 0;

  for(uint8_t y=FH; y<8*FH; y+=FH)
  {
    uint8_t i=(y/FH)+s_pgOfs-1;
    uint8_t attr = sub==i ? INVERS : 0;
    switch(i){
      case 0:
        lcd_putsAtt(  2*FW,y,PSTR("Source"),0);
        putsChnRaw(   FW*10,y,md2->srcRaw,attr);
        if(attr) CHECK_INCDEC_H_MODELVAR( event, md2->srcRaw, 1,NUM_XCHNRAW);
        break;
      case 1:
        lcd_putsAtt(  2*FW,y,PSTR("Weight"),0);
        lcd_outdezAtt(10*FW+NUM_OFS(md2->weight),y,md2->weight,attr);
        if(attr) CHECK_INCDEC_H_MODELVAR( event, md2->weight, -125,125);
        break;
      case 2:
        lcd_putsAtt(  2*FW,y,PSTR("Offset"),0);
        lcd_outdezAtt(10*FW+NUM_OFS(md2->sOffset),y,md2->sOffset,attr);
        if(attr) CHECK_INCDEC_H_MODELVAR( event, md2->sOffset, -125,125);
        break;
      case 3:
        lcd_putsAtt(  2*FW,y,PSTR("Trim"),0);
        lcd_putsnAtt(FW*10,y, PSTR("ON OFF")+3*md2->carryTrim,3,attr);  //default is 0=ON
        if(attr) CHECK_INCDEC_H_MODELVAR_BF( event, md2->carryTrim, 0,1);
        break;
      case 4:
        lcd_putsAtt(  2*FW,y,PSTR("Curves"),0);
        lcd_putsnAtt( FW*10,y,PSTR(CURV_STR)+md2->curve*3,3,attr);
        if(attr) CHECK_INCDEC_H_MODELVAR( event, md2->curve, 0,MAX_CURVE5+MAX_CURVE9+7-1);
        if(attr && md2->curve>=CURVE_BASE && event==EVT_KEY_FIRST(KEY_MENU)){
          s_curveChan = md2->curve-CURVE_BASE;
          pushMenu(menuProcCurveOne);
        }
        break;
      case 5:
        lcd_putsAtt(  2*FW,y,PSTR("Switch"),0);
        putsDrSwitches(9*FW,  y,md2->swtch,attr);
        if(attr) CHECK_INCDEC_H_MODELVAR( event, md2->swtch, -MAX_DRSWITCH, MAX_DRSWITCH);
        break;
      case 6:
        lcd_putsAtt(  2*FW,y,PSTR("Warning"),0);
        if(md2->mixWarn)
          lcd_outdezAtt(FW*10+NUM_OFS(md2->mixWarn),y,md2->mixWarn,attr);
        else
          lcd_putsAtt(  FW*10,y,PSTR("OFF"),attr);
        if(attr) CHECK_INCDEC_H_MODELVAR_BF( event, md2->mixWarn, 0,3);
        break;
      case 7:
        lcd_putsAtt(  2*FW,y,PSTR("Multpx"),0);
        lcd_putsnAtt(10*FW, y,PSTR("Add     MultiplyReplace ")+8*md2->mltpx,8,attr);
        if(attr) CHECK_INCDEC_H_MODELVAR_BF( event, md2->mltpx, 0, 2); //!! bitfield
        break;
      case 8:
        lcd_putsAtt(  2*FW,y,PSTR("Delay Down"),0);
        attr = (sub==i) ? INVERS : 0;
        lcd_outdezAtt(FW*16,y,md2->delayDown,attr);
        if(attr)  CHECK_INCDEC_H_MODELVAR_BF( event, md2->delayDown, 0,15); //!! bitfield
        break;
      case 9:
        lcd_putsAtt(  2*FW,y,PSTR("Delay Up"),0);
        attr = (sub==i) ? INVERS : 0;
        lcd_outdezAtt(FW*16,y,md2->delayUp,attr);
        if(attr)  CHECK_INCDEC_H_MODELVAR_BF( event, md2->delayUp, 0,15); //!! bitfield
        break;
      case 10:
        lcd_putsAtt(  2*FW,y,PSTR("Slow  Down"),0);
        attr = (sub==i) ? INVERS : 0;
        lcd_outdezAtt(FW*16,y,md2->speedDown,attr);
        if(attr)  CHECK_INCDEC_H_MODELVAR_BF( event, md2->speedDown, 0,15); //!! bitfield
        break;
      case 11:
        lcd_putsAtt(  2*FW,y,PSTR("Slow  Up"),0);
        attr = (sub==i) ? INVERS : 0;
        lcd_outdezAtt(FW*16,y,md2->speedUp,attr);
        if(attr)  CHECK_INCDEC_H_MODELVAR_BF( event, md2->speedUp, 0,15); //!! bitfield
        break;
      case 12:   lcd_putsAtt(  2*FW,y,PSTR("DELETE MIX [MENU]"),attr);
        if(attr && event==EVT_KEY_LONG(KEY_MENU)){
          killEvents(event);
          deleteMix(s_currMixIdx);
          beepWarn1();
          popMenu();
        }
        break;
    }
  }
}

struct MixTab{
  bool   showCh:1;// show the dest chn
  bool   hasDat:1;// show the data info
  int8_t chId;    //:4  1..NUM_XCHNOUT  dst chn id
  int8_t selCh;   //:5  1..MAX_MIXERS+NUM_XCHNOUT sel sequence
  int8_t selDat;  //:5  1..MAX_MIXERS+NUM_XCHNOUT sel sequence
  int8_t insIdx;  //:5  0..MAX_MIXERS-1        insert index into mix data tab
  int8_t editIdx; //:5  0..MAX_MIXERS-1        edit   index into mix data tab
} s_mixTab[MAX_MIXERS+NUM_XCHNOUT+1];
int8_t s_mixMaxSel;

void genMixTab()
{
  uint8_t maxDst  = 0;
  uint8_t mtIdx   = 0;
  uint8_t sel     = 1;
  memset(s_mixTab,0,sizeof(s_mixTab));

  MixData *md=g_model.mixData;

  for(uint8_t i=0; i<MAX_MIXERS; i++)
  {
    uint8_t destCh = md[i].destCh;
    if(destCh==0) destCh=NUM_XCHNOUT;
    if(destCh > maxDst){
      while(destCh > maxDst){ //ch-loop, hole alle channels auf
        maxDst++;
        s_mixTab[mtIdx].chId  = maxDst; //mark channel header
        s_mixTab[mtIdx].showCh = true;
        s_mixTab[mtIdx].selCh = sel++; //vorab vergeben, falls keine dat
        s_mixTab[mtIdx].insIdx= i;     //
        mtIdx++;
      }
      mtIdx--; //folding: letztes ch bekommt zusaetzlich dat
      s_mixMaxSel =sel;
      sel--; //letzte zeile hat dat, falls nicht ist selCh schon belegt
    }
    if(md[i].destCh==0) break; //letzter eintrag in mix data tab
    s_mixTab[mtIdx].chId    = destCh; //mark channel header
    s_mixTab[mtIdx].editIdx = i;
    s_mixTab[mtIdx].hasDat  = true;
    s_mixTab[mtIdx].selDat  = sel++;
    if(md[i].destCh == md[i+1].destCh){
      s_mixTab[mtIdx].selCh  = 0; //ueberschreibt letzte Zeile von ch-loop
      s_mixTab[mtIdx].insIdx = 0; //
    }
    else{
      s_mixTab[mtIdx].selCh  = sel++;
      s_mixTab[mtIdx].insIdx = i+1; //
    }
    s_mixMaxSel =sel;
    mtIdx++;
  }
}

static void memswap( void *a, void *b, uint8_t size )
{
	uint8_t *x ;
	uint8_t *y ;
	uint8_t temp ;

	x = (unsigned char *) a ;
	y = (unsigned char *) b ;
	while ( size-- )
	{
		temp = *x ;
		*x++ = *y ;
		*y++ = temp ;
	}
}

void moveMix(uint8_t idx,uint8_t mcopy, uint8_t dir) //true=inc=down false=dec=up - Issue 49
{
  if(idx>MAX_MIXERS || (idx==0 && !dir) || (idx==MAX_MIXERS && dir)) return;
  uint8_t tdx = dir ? idx+1 : idx-1;
  MixData *src= mixaddress( idx ) ; //&g_model.mixData[idx];
  MixData *tgt= mixaddress( tdx ) ; //&g_model.mixData[tdx];

  if((src->destCh==0) || (src->destCh>NUM_CHNOUT) || (tgt->destCh>NUM_CHNOUT)) return;

  if(mcopy) {

    if (reachMixerCountLimit())
    {
      return;
    }

		memmove(&g_model.mixData[idx+1],&g_model.mixData[idx],
         (MAX_MIXERS-(idx+1))*sizeof(MixData) );
    return;
  }

  if(tgt->destCh!=src->destCh) {
    if ((dir)  && (src->destCh<NUM_CHNOUT)) src->destCh++;
    if ((!dir) && (src->destCh>0))          src->destCh--;
    return;
  }

  //flip between idx and tgt
	memswap( tgt, src, sizeof(MixData) ) ;
  STORE_MODELVARS;

}

void menuMixersLimit(uint8_t event)
{
  lcd_putsAtt(0,2*FH, PSTR("Max mixers reach: "),0);
  lcd_outdezAtt(20*FW, 2*FH, getMixerCount(),0);
  
  lcd_puts_P(0,4*FH, PSTR("Press [EXIT] to abort"));
  switch(event)
  {
    case  EVT_KEY_FIRST(KEY_EXIT):
      killEvents(event);
      popMenu(true);
      pushMenu(menuProcMix);
    break;
  }
}

uint8_t getMixerCount()
{
  uint8_t mixerCount = 0;
	uint8_t dch ;

  for(uint8_t i=0;i<MAX_MIXERS;i++)
  {
		dch = g_model.mixData[i].destCh ;
    if ((dch!=0) && (dch<=NUM_CHNOUT)) 
    {
      mixerCount++;
    }
  }
  return mixerCount;
}

bool reachMixerCountLimit()
{
  // check mixers count limit
  if (getMixerCount() >= MAX_MIXERS)
  {
    pushMenu(menuMixersLimit);
    return true;
  }
  else
  {
    return false;
  }
}

void menuProcMix(uint8_t event)
{
  static MState2 mstate2;
  TITLE("MIXER");
  MSTATE_CHECK_V(5,menuTabModel,s_mixMaxSel);
  int8_t  sub    = mstate2.m_posVert;
  static uint8_t s_moveMode;
  static uint8_t s_sourceMoveMix;
  static uint8_t s_copyMix;
  if(sub==0) s_moveMode = false;

  MixData *md=g_model.mixData;
  switch(event)
  {
    case EVT_ENTRY:
      s_pgOfs=0;
      s_sourceMoveMix=0;
      s_copyMix=true;
      s_moveMode=false;
    case EVT_ENTRY_UP:
      genMixTab();
      break;
    case EVT_KEY_FIRST(KEY_MENU):
      if(sub>=1) s_moveMode = !s_moveMode;
      s_copyMix = s_moveMode;
      if(s_currMixInsMode) s_moveMode = false; //cant have edit mode if mix not selected
      break;
    case EVT_KEY_REPT(KEY_UP):
    case EVT_KEY_FIRST(KEY_UP):
      s_copyMix = false;  //only copy if going down
    case EVT_KEY_REPT(KEY_DOWN):
    case EVT_KEY_FIRST(KEY_DOWN):
      if(s_moveMode) {
        //killEvents(event);
        moveMix(s_currMixIdx,s_copyMix,event==EVT_KEY_FIRST(KEY_DOWN)); //true=inc=down false=dec=up
        if(s_copyMix && event==EVT_KEY_FIRST(KEY_UP)) sub--;
        s_copyMix = false;
        genMixTab();
      }
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      if(s_moveMode) killEvents(event);
      s_moveMode = false;

      //need to delete source of mix
      break;
    case EVT_KEY_LONG(KEY_MENU):
      if(sub<1) break;
      if (s_currMixInsMode && !reachMixerCountLimit())
      {
        insertMix(s_currMixIdx);
      }
      s_moveMode=false;
      pushMenu(menuProcMixOne);
      break;
  }

  int8_t markedIdx=-1;
  uint8_t i;
  int8_t minSel=99;
  int8_t maxSel=-1;
//  lcd_puts_P( 6*FW, 1*FH,PSTR("wt src  sw crv"));
  for(i=0; i<7; i++){
    uint8_t y = i * FH + 1*FH;
    uint8_t k = i + s_pgOfs;
    if(!s_mixTab[k].showCh && !s_mixTab[k].hasDat ) break;

    if(s_mixTab[k].showCh){
      putsChn(0,y,s_mixTab[k].chId, 0); // show CHx
    }
    if(sub>0 && sub==s_mixTab[k].selCh) { //handle CHx is selected (other line)
      //if(BLINK_ON_PHASE)
      lcd_hline(0,y+7,FW*4);
      s_currMixIdx     = s_mixTab[k].insIdx;
      s_currDestCh     = s_mixTab[k].chId;
      s_currMixInsMode = true;
      markedIdx        = i;
    }
    if(s_mixTab[k].hasDat){ //show data
      MixData *md2=&md[s_mixTab[k].editIdx];
      uint8_t attr = sub==s_mixTab[k].selDat ? INVERS : 0;
      if(!s_mixTab[k].showCh) //show prefix only if not first mix
          lcd_putsnAtt(   3*FW, y, PSTR("+*R")+1*md2->mltpx,1,s_moveMode ? attr : 0);
      lcd_outdezAtt(  7*FW+FW/2, y, md2->weight,attr);
      lcd_putcAtt(    7*FW+FW/2, y, '%',s_moveMode ? attr : 0);
      putsChnRaw(     9*FW, y, md2->srcRaw,s_moveMode ? attr : 0);
      if(md2->swtch)putsDrSwitches( 13*FW, y, md2->swtch,s_moveMode ? attr : 0);
      if(md2->curve)lcd_putsnAtt(   17*FW, y, PSTR(CURV_STR)+md2->curve*3,3,s_moveMode ? attr : 0);

      bool bs = (md2->speedDown || md2->speedUp);
      bool bd = (md2->delayUp || md2->delayDown);
      char cs = (bs && bd) ? '*' : (bs ? 'S' : 'D');
      if(bs || bd) lcd_putcAtt(20*FW+1, y, cs,s_editMode ? attr : 0);

      if(attr == INVERS){ //handle dat is selected
        CHECK_INCDEC_H_MODELVAR( event, md2->weight, -125,125);
        s_currMixIdx     = s_mixTab[k].editIdx;
        s_currDestCh     = s_mixTab[k].chId;
        s_currMixInsMode = false;
        markedIdx        = i;
        if(!s_moveMode) s_sourceMoveMix = k;
      }
    }
    //welche sub-indize liegen im sichtbaren bereich?
    if(s_mixTab[k].selCh){
      minSel = min(minSel,s_mixTab[k].selCh);
      maxSel = max(maxSel,s_mixTab[k].selCh);
    }
    if(s_mixTab[k].selDat){
      minSel = min(minSel,s_mixTab[k].selDat);
      maxSel = max(maxSel,s_mixTab[k].selDat);
    }

  } //for 7
  if( sub!=0 &&  markedIdx==-1) { //versuche die Marke zu finden
    if(sub < minSel) s_pgOfs = max(0,s_pgOfs-1);
    if(sub > maxSel) s_pgOfs++;
  }
  else if(markedIdx<0)              s_pgOfs = max(0,s_pgOfs-1);
  else if(markedIdx>=6 && i>=8)      s_pgOfs++;

}



uint16_t expou(uint16_t x, uint16_t k)
{
  // k*x*x*x + (1-k)*x
  return ((unsigned long)x*x*x/0x10000*k/(RESXul*RESXul/0x10000) + (RESKul-k)*x+RESKul/2)/RESKul;
}
// expo-funktion:
// ---------------
// kmplot
// f(x,k)=exp(ln(x)*k/10) ;P[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20]
// f(x,k)=x*x*x*k/10 + x*(1-k/10) ;P[0,1,2,3,4,5,6,7,8,9,10]
// f(x,k)=x*x*k/10 + x*(1-k/10) ;P[0,1,2,3,4,5,6,7,8,9,10]
// f(x,k)=1+(x-1)*(x-1)*(x-1)*k/10 + (x-1)*(1-k/10) ;P[0,1,2,3,4,5,6,7,8,9,10]

int16_t expo(int16_t x, int16_t k)
{
  if(k == 0) return x;
  int16_t   y;
  bool    neg =  x < 0;
  if(neg)   x = -x;
  if(k<0){
    y = RESXu-expou(RESXu-x,-k);
  }else{
    y = expou(x,k);
  }
  return neg? -y:y;
}


#ifdef EXTENDED_EXPO
/// expo with y-offset
class Expo
{
  uint16_t   c;
  int16_t    d,drx;
public:
  void     init(uint8_t k, int8_t yo);
  static int16_t  expou(uint16_t x,uint16_t c, int16_t d);
  int16_t  expo(int16_t x);
};
void    Expo::init(uint8_t k, int8_t yo)
{
  c = (uint16_t) k  * 256 / 100;
  d = (int16_t)  yo * 256 / 100;
  drx = d * ((uint16_t)RESXu/256);
}
int16_t Expo::expou(uint16_t x,uint16_t c, int16_t d)
{
  uint16_t a = 256 - c - d;
  if( (int16_t)a < 0 ) a = 0;
  // a x^3 + c x + d
  //                         9  18  27        11  20   18
  uint32_t res =  ((uint32_t)x * x * x / 0x10000 * a / (RESXul*RESXul/0x10000) +
                   (uint32_t)x                   * c
  ) / 256;
  return (int16_t)res;
}
int16_t  Expo::expo(int16_t x)
{
  if(c==256 && d==0) return x;
  if(x>=0) return expou(x,c,d) + drx;
  return -expou(-x,c,-d) + drx;
}
#endif

static uint8_t s_expoChan;

void editExpoVals(uint8_t event,uint8_t stopBlink,uint8_t editMode, uint8_t edit,uint8_t x, uint8_t y, uint8_t chn, uint8_t which, uint8_t exWt, uint8_t stkRL)
{
  uint8_t  invBlk = edit ? (editMode ? BLINK : INVERS) : 0;
  if(edit && stopBlink) invBlk = INVERS;

  if(which==DR_DRSW1) {
    putsDrSwitches(x,y,g_model.expoData[chn].drSw1,invBlk);
    if(edit && (editMode || p1valdiff)) CHECK_INCDEC_H_MODELVAR(event,g_model.expoData[chn].drSw1,-MAX_DRSWITCH,MAX_DRSWITCH);
  }
  else if(which==DR_DRSW2) {
    putsDrSwitches(x,y,g_model.expoData[chn].drSw2,invBlk);
    if(edit && (editMode || p1valdiff)) CHECK_INCDEC_H_MODELVAR(event,g_model.expoData[chn].drSw2,-MAX_DRSWITCH,MAX_DRSWITCH);
  }
  else
    if(exWt==DR_EXPO){
      lcd_outdezAtt(x, y, g_model.expoData[chn].expo[which][exWt][stkRL], invBlk);
      if(edit && (editMode || p1valdiff)) CHECK_INCDEC_H_MODELVAR(event,g_model.expoData[chn].expo[which][exWt][stkRL],-100, 100);
    }
    else {
      lcd_outdezAtt(x, y, g_model.expoData[chn].expo[which][exWt][stkRL]+100, invBlk);
      if(edit && (editMode || p1valdiff)) CHECK_INCDEC_H_MODELVAR(event,g_model.expoData[chn].expo[which][exWt][stkRL],-100, 0);
    }
}

void menuProcExpoOne(uint8_t event)
{
  static MState2 mstate2;
  static uint8_t stkVal;
  uint8_t x=TITLE("EXPO/DR ");
  putsChnRaw(x,0,s_expoChan+1,0);
  MSTATE_CHECK0_V(4);
  int8_t  sub    = mstate2.m_posVert;

   switch(event)
  {
      case EVT_ENTRY:
          s_editMode = false;
          break;
      case EVT_KEY_FIRST(KEY_MENU):
          s_editMode = false;
          break;
  }
  uint8_t expoDrOn = GET_DR_STATE(s_expoChan);
  uint8_t  y = 16;

  if(calibratedStick[s_expoChan]> 25) stkVal = DR_RIGHT;
  if(calibratedStick[s_expoChan]<-25) stkVal = DR_LEFT;
  if(IS_THROTTLE(s_expoChan) && g_model.thrExpo) stkVal = DR_RIGHT;

  lcd_puts_P(0,y,PSTR("Expo"));
  editExpoVals(event,true,true,sub==0,9*FW, y,s_expoChan, expoDrOn ,DR_EXPO,stkVal);
  y+=FH;
  lcd_puts_P(0,y,PSTR("Weight"));
  editExpoVals(event,true,true,sub==1,9*FW, y,s_expoChan, expoDrOn ,DR_WEIGHT,stkVal);
  y+=FH;
  lcd_puts_P(0,y,PSTR("DrSw1"));
  editExpoVals(event,true,true,sub==2,5*FW, y,s_expoChan, DR_DRSW1 , 0,0);
  y+=FH;
  lcd_puts_P(0,y,PSTR("DrSw2"));
  editExpoVals(event,true,true,sub==3,5*FW, y,s_expoChan, DR_DRSW2 , 0,0);
  y+=FH;
  switch (expoDrOn) {
    case DR_MID:
      lcd_puts_P(0,y,PSTR("DR Mid"));
      break;
    case DR_LOW:
      lcd_puts_P(0,y,PSTR("DR Low"));
      break;
    default: // DR_HIGH:
      lcd_puts_P(0,y,PSTR("DR High"));
      break;
  }
  y+=FH;


  int8_t   kViewR  = g_model.expoData[s_expoChan].expo[expoDrOn][DR_EXPO][DR_RIGHT];  //NormR;
  int8_t   kViewL  = g_model.expoData[s_expoChan].expo[expoDrOn][DR_EXPO][DR_LEFT];  //NormL;
  int8_t   wViewR  = g_model.expoData[s_expoChan].expo[expoDrOn][DR_WEIGHT][DR_RIGHT]+100;  //NormWeightR+100;
  int8_t   wViewL  = g_model.expoData[s_expoChan].expo[expoDrOn][DR_WEIGHT][DR_LEFT]+100;  //NormWeightL+100;

  if (IS_THROTTLE(s_expoChan) && g_model.thrExpo)
       for(uint8_t xv=0;xv<WCHARTl*2;xv++)
    {
      uint16_t yv=2*expo(xv*(RESXu/WCHARTl)/2,kViewR) / (RESXu/WCHARTl);
      yv = (yv * wViewR)/100;
      lcd_plot(X0l+xv-WCHARTl, 2*Y0l-yv);
      if((xv&3) == 0){
        lcd_plot(X0l+xv-WCHARTl, 2*Y0l-1);
        lcd_plot(X0l-WCHARTl   , Y0l+xv/2);
      }
    }
  else
    for(uint8_t xv=0;xv<WCHARTl;xv++)
    {
      uint16_t yv=expo(xv*(RESXu/WCHARTl),kViewR) / (RESXu/WCHARTl);
      yv = (yv * wViewR)/100;
      lcd_plot(X0l+xv, Y0l-yv);
      if((xv&3) == 0){
        lcd_plot(X0l+xv, Y0l+0);
        lcd_plot(X0l  , Y0l+xv);
      }

      yv=expo(xv*(RESXu/WCHARTl),kViewL) / (RESXu/WCHARTl);
      yv = (yv * wViewL)/100;
      lcd_plot(X0l-xv, Y0l+yv);
      if((xv&3) == 0){
        lcd_plot(X0l-xv, Y0l+0);
        lcd_plot(X0l  , Y0l-xv);
      }
    }

  int32_t x512  = calibratedStick[s_expoChan];
  lcd_vline(X0l+x512/(RESXu/WCHARTl), Y0l-WCHARTl,WCHARTl*2);

  int32_t y512 = 0;
  if (IS_THROTTLE(s_expoChan) && g_model.thrExpo) {
    y512  = 2*expo((x512+RESX)/2,kViewR);
    y512 = y512 * (wViewR / 4)/(100 / 4);
    lcd_hline(X0l-WCHARTl, 2*Y0l-y512/(RESXu/WCHARTl),WCHARTl*2);
    y512 /= 2;
  }
  else {
    y512  = expo(x512,(x512>0 ? kViewR : kViewL));
    y512 = y512 * ((x512>0 ? wViewR : wViewL) / 4)/(100 / 4);
    lcd_hline(X0l-WCHARTl, Y0l-y512/(RESXu/WCHARTl),WCHARTl*2);
  }

  lcd_outdezAtt( 19*FW, 6*FH,x512*25/((signed) RESXu/4), 0 );
  lcd_outdezAtt( 14*FW, 1*FH,y512*25/((signed) RESXu/4), 0 );
  //dy/dx
  int16_t dy  = x512>0 ? y512-expo(x512-20,(x512>0 ? kViewR : kViewL)) : expo(x512+20,(x512>0 ? kViewR : kViewL))-y512;
  lcd_outdezNAtt(14*FW, 2*FH,   dy*(100/20), LEADING0|PREC2,3);
}

void menuProcExpoAll(uint8_t event)
{
  static MState2 mstate2;
  static uint8_t stkVal[4];
  TITLE("EXPO/DR");
  MSTATE_TAB = {5,4};
  MSTATE_CHECK_VxH(4,menuTabModel,4+1);
  int8_t  sub    = mstate2.m_posVert - 1;
  int8_t  subHor = mstate2.m_posHorz;

  switch(event)
  {
    case EVT_ENTRY:
      s_editMode = false;
      break;
    case EVT_KEY_FIRST(KEY_MENU):
      if(sub>=0) s_editMode = !s_editMode;
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      if(s_editMode) {
        s_editMode = false;
        killEvents(event);
      }
      break;
    case EVT_KEY_LONG(KEY_MENU):
      if(sub>=0){
        s_expoChan = sub;
        pushMenu(menuProcExpoOne);
      }
      break;
  }

  lcd_puts_P( 4*FW-FW/2, 1*FH,PSTR("exp  %  sw1 sw2"));
  for(uint8_t i=0; i<4; i++)
  {
    uint8_t expoDrOn = GET_DR_STATE(i);
    uint8_t valsEqual = (g_model.expoData[i].expo[expoDrOn][DR_WEIGHT][DR_LEFT]==g_model.expoData[i].expo[expoDrOn][DR_WEIGHT][DR_RIGHT]) &&
                        (g_model.expoData[i].expo[expoDrOn][DR_EXPO][DR_LEFT]==g_model.expoData[i].expo[expoDrOn][DR_EXPO][DR_RIGHT]);
    uint8_t stickCentred = (abs(calibratedStick[i])<=25) && valsEqual;
    if(calibratedStick[i]> 25) stkVal[i] = DR_RIGHT;
    if(calibratedStick[i]<-25) stkVal[i] = DR_LEFT;
    if(IS_THROTTLE(i) && g_model.thrExpo) {
      stkVal[i] = DR_RIGHT;
      stickCentred = true;
    }

    uint8_t y=(i+2)*FH;
    putsChnRaw( 0, y,i+1,0);
    uint8_t stkOp = (stkVal[i] == DR_RIGHT) ? DR_LEFT : DR_RIGHT;

    uint8_t edtm = (s_editMode || p1valdiff);
    editExpoVals(event,false,edtm,sub==i && subHor==0, 7*FW-FW/2, y,i,expoDrOn,DR_EXPO,stkVal[i]);
    if(sub==i && subHor==0 && edtm && stickCentred)
      CHECK_INCDEC_H_MODELVAR(event,g_model.expoData[i].expo[expoDrOn][DR_EXPO][stkOp],-100, 100);

    editExpoVals(event,false,edtm,sub==i && subHor==1, 9*FW+FW/2, y,i,expoDrOn,DR_WEIGHT,stkVal[i]);
    if(sub==i && subHor==1 && edtm && stickCentred)
      CHECK_INCDEC_H_MODELVAR(event,g_model.expoData[i].expo[expoDrOn][DR_WEIGHT][stkOp],-100, 0);

    editExpoVals(event,false,edtm,sub==i && subHor==2,10*FW+FW/2, y,i,DR_DRSW1,0,0);
    editExpoVals(event,false,edtm,sub==i && subHor==3,14*FW+FW/2, y,i,DR_DRSW2,0,0);
    lcd_putcAtt(9*FW+FW/2 + ((!stkVal[i] && !stickCentred) ? 2 : 1 ), y, stickCentred ? '-' : (stkVal[i] ? 127 : 126),0);//'|' : (stkVal[i] ? '<' : '>'),0);
    switch (expoDrOn) {
    case DR_MID:
      lcd_putcAtt(19*FW+FW/2,y,'M',0);
      break;
    case DR_LOW:
      lcd_putcAtt(19*FW+FW/2,y,'L',0);
      break;
    default: // DR_HIGH:
      lcd_putcAtt(19*FW+FW/2,y,'H',0);
      break;
    }
  }
}

uint8_t char2idx(char c)
{
  for(int8_t ret=0;;ret++)
  {
    char cc= pgm_read_byte(s_charTab+ret);
    if(cc==c) return ret;
    if(cc==0) return 0;
  }
}
char idx2char(uint8_t idx)
{
  if(idx < NUMCHARS) return pgm_read_byte(s_charTab+idx);
  return ' ';
}

uint8_t DupIfNonzero = 0 ;
int8_t DupSub ;

void menuDeleteDupModel(uint8_t event)
//void menuDeleteModel(uint8_t event)
{
  lcd_putsAtt(0,1*FH,DupIfNonzero ? PSTR("DUPLICATE MODEL") : PSTR("DELETE MODEL"),0);
//  lcd_putsAtt(0,1*FH,PSTR("DELETE MODEL"),0);
  lcd_putsnAtt(1,2*FH, g_model.name,sizeof(g_model.name),BSS_NO_INV);
  lcd_putcAtt(sizeof(g_model.name)*FW+FW,2*FH,'?',0);
  lcd_puts_P(3*FW,5*FH,PSTR("YES     NO"));
  lcd_puts_P(3*FW,6*FH,PSTR("[MENU]  [EXIT]"));

  uint8_t i;
  switch(event){
    case EVT_ENTRY:
      beepWarn();
      break;
    case EVT_KEY_FIRST(KEY_MENU):
			if ( DupIfNonzero )
			{
        message(PSTR("Duplicating model"));
        if(eeDuplicateModel(DupSub))
				{
          beepKey();
					DupIfNonzero = 2 ;		// sel_editMode = false;
        }
        else beepWarn();
			}
			else
			{
        EFile::rm(FILE_MODEL(g_eeGeneral.currModel)); //delete file

        i = g_eeGeneral.currModel;//loop to find next available model
        while (!EFile::exists(FILE_MODEL(i))) {
            i--;
            if(i>MAX_MODELS) i=MAX_MODELS-1;
            if(i==g_eeGeneral.currModel) {
                i=0;
                break;
            }
        }
        g_eeGeneral.currModel = i;
        STORE_GENERALVARS;

        eeLoadModel(g_eeGeneral.currModel); //load default values
        resetTimer();
			}
      killEvents(event);
      popMenu(true);
      pushMenu(menuProcModelSelect);
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      killEvents(event);
      popMenu(true);
      pushMenu(menuProcModelSelect);
      break;
  }
}

void menuProcModel(uint8_t event)
{
  static MState2 mstate2;
  uint8_t x=TITLE("SETUP ");
  lcd_outdezNAtt(x+2*FW,0,g_eeGeneral.currModel+1,INVERS+LEADING0,2);
  MSTATE_TAB = { 1,sizeof(g_model.name),2,1,1,1,1,1,1,7,3,1,1,1,1};
  MSTATE_CHECK_VxH(2,menuTabModel,15);
  int8_t  sub    = mstate2.m_posVert;
  uint8_t subSub = mstate2.m_posHorz + 1;

  if(sub<1) s_pgOfs=0;
  else if((sub-s_pgOfs)>7) s_pgOfs = sub-7;
  else if((sub-s_pgOfs)<1) s_pgOfs = sub-1;
  if(s_pgOfs<0) s_pgOfs = 0;

  uint8_t y = 1*FH;

  switch(event){
    case EVT_ENTRY:
      s_editMode = false;
      break;
    case EVT_KEY_FIRST(KEY_MENU):
      if(sub>=0) s_editMode = !s_editMode;
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      if(s_editMode) {
        s_editMode = false;
        killEvents(event);
      }
      break;
    case EVT_KEY_REPT(KEY_LEFT):
    case EVT_KEY_FIRST(KEY_LEFT):
      if(sub==1 && subSub>1 && s_editMode) mstate2.m_posHorz--;
      break;
    case EVT_KEY_REPT(KEY_RIGHT):
    case EVT_KEY_FIRST(KEY_RIGHT):
      if(sub==1 && subSub<sizeof(g_model.name) && s_editMode) mstate2.m_posHorz++;
      break;
    case EVT_KEY_REPT(KEY_UP):
    case EVT_KEY_FIRST(KEY_UP):
    case EVT_KEY_REPT(KEY_DOWN):
    case EVT_KEY_FIRST(KEY_DOWN):
      if (!s_editMode) mstate2.m_posHorz = 0;
      break;
  }

  uint8_t subN = 1;
  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Name"),0);
    lcd_putsnAtt(10*FW,   y, g_model.name ,sizeof(g_model.name),BSS_NO_INV | (sub==subN ? (s_editMode ? 0 : INVERS) : 0));
    if(sub==subN && s_editMode){
        char v = char2idx(g_model.name[subSub-1]);
        if(p1valdiff || event==EVT_KEY_FIRST(KEY_DOWN) || event==EVT_KEY_FIRST(KEY_UP) || event==EVT_KEY_REPT(KEY_DOWN) || event==EVT_KEY_REPT(KEY_UP))
           CHECK_INCDEC_H_MODELVAR_BF( event,v ,0,NUMCHARS-1);
        v = idx2char(v);
        g_model.name[subSub-1]=v;
        lcd_putcAtt((10+subSub-1)*FW, y, v,INVERS);
    }
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Timer"),0);
    putsTime(       9*FW, y, g_model.tmrVal,(sub==subN && subSub==1 ? (s_editMode ? BLINK : INVERS):0),(sub==subN && subSub==2 ? (s_editMode ? BLINK : INVERS):0) );

    if(sub==subN && (s_editMode || p1valdiff))
      switch (subSub) {
       case 1:
          {
          int8_t min=g_model.tmrVal/60;
          CHECK_INCDEC_H_MODELVAR_BF( event,min ,0,59);
          g_model.tmrVal = g_model.tmrVal%60 + min*60;
         break;
          }
        case 2:
          {
          int8_t sec=g_model.tmrVal%60;
          sec -= checkIncDec_hm( event,sec+2 ,1,62)-2;
          g_model.tmrVal -= sec ;
          if((int16_t)g_model.tmrVal < 0) g_model.tmrVal=0;
          break;
          }
      }
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) { //timer trigger source -> off, abs, stk, stk%, sw/!sw, !m_sw/!m_sw, chx(value > or < than tmrChVal), ch%
    lcd_putsAtt(    0,    y, PSTR("Trigger"),0);
    uint8_t attr = (sub==subN ?  INVERS : 0);
    putsTmrMode(10*FW,y,attr);

    if(sub==subN)
      CHECK_INCDEC_H_MODELVAR( event,g_model.tmrMode ,-(13+2*MAX_DRSWITCH),(13+2*MAX_DRSWITCH));
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Timer "),0);
    lcd_putsnAtt(  10*FW, y, PSTR("Count DownCount Up  ")+10*g_model.tmrDir,10,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event,g_model.tmrDir,0,1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("T-Trim"),0);
    lcd_putsnAtt(  10*FW, y, PSTR("OFFON ")+3*g_model.thrTrim,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event,g_model.thrTrim,0,1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("T-Expo"),0);
    lcd_putsnAtt(  10*FW, y, PSTR("OFFON ")+3*g_model.thrExpo,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event,g_model.thrExpo,0,1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Trim Inc"),0);
    lcd_putsnAtt(  10*FW, y, PSTR("Exp   ExFineFine  MediumCoarse")+6*g_model.trimInc,6,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR(event,g_model.trimInc,0,4);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Trim Sw"),0);
    putsDrSwitches(9*FW,y,g_model.trimSw,sub==subN ? INVERS:0);
    if(sub==subN) CHECK_INCDEC_H_MODELVAR(event,g_model.trimSw,-MAX_DRSWITCH, MAX_DRSWITCH);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Beep Cnt"),0);
    for(uint8_t i=0;i<7;i++) lcd_putsnAtt((10+i)*FW, y, PSTR("RETA123")+i,1, (((subSub-1)==i) && (sub==subN)) ? BLINK : ((g_model.beepANACenter & (1<<i)) ? INVERS : 0 ) );
    if(sub==subN){
        if((event==EVT_KEY_FIRST(KEY_MENU)) || p1valdiff) {
            killEvents(event);
            s_editMode = false;
            g_model.beepANACenter ^= (1<<(subSub-1));
            STORE_MODELVARS;
        }
    }
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Proto"),0);//sub==2 ? INVERS:0);
    lcd_putsnAtt(  6*FW, y, PSTR(PROT_STR)+PROT_STR_LEN*g_model.protocol,PROT_STR_LEN,
                  (sub==subN && subSub==1 ? (s_editMode ? BLINK : INVERS):0));
    switch (g_model.protocol){
      case PROTO_PPM:
        lcd_putsnAtt(  10*FW, y, PSTR("4CH 6CH 8CH 10CH12CH14CH16CH")+4*(g_model.ppmNCH+2),4,(sub==subN && subSub==2  ? (s_editMode ? BLINK : INVERS):0));
        lcd_putsAtt(    17*FW,    y, PSTR("uSec"),0);
        lcd_outdezAtt(  17*FW, y,  (g_model.ppmDelay*50)+300, (sub==subN && subSub==3 ? (s_editMode ? BLINK : INVERS):0));

        if(sub==subN && (s_editMode || p1valdiff))
          switch (subSub){
            case 1:
                CHECK_INCDEC_H_MODELVAR(event,g_model.protocol,0,PROT_MAX);
                break;
            case 2:
                CHECK_INCDEC_H_MODELVAR(event,g_model.ppmNCH,-2,4);
                break;
            case 3:
                CHECK_INCDEC_H_MODELVAR(event,g_model.ppmDelay,-4,10);
                break;
          }
        break;
      case PROTO_PPRZ_XBEE_API:
      case PROTO_PPRZ_TRANSPARENT:
      case PROTO_PPRZ_LAIRD:
        if (g_model.ppmNCH > 0) g_model.ppmNCH = 0;
        lcd_putsnAtt(  13*FW, y, PSTR("3CH 4CH 8CH ")+4*(g_model.ppmNCH+2),4,(sub==subN && subSub==2  ? (s_editMode ? BLINK : INVERS):0));

        if(sub==subN && (s_editMode || p1valdiff))
          switch (subSub){
            case 1:
                CHECK_INCDEC_H_MODELVAR(event,g_model.protocol,0,PROT_MAX);
                break;
            case 2:
                CHECK_INCDEC_H_MODELVAR(event,g_model.ppmNCH,-2,0);
                break;
          }
        break;
      default:
        if(sub==subN && (s_editMode || p1valdiff))
          switch (subSub){
            case 1:
                CHECK_INCDEC_H_MODELVAR(event,g_model.protocol,0,PROT_MAX);
                break;
          }
        break;
    }
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Shift Sel"),0);
    lcd_putsnAtt(  10*FW, y, PSTR("POSNEG")+3*g_model.pulsePol,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event,g_model.pulsePol,0,1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("E. Limits"),0);
    lcd_putsnAtt(  10*FW, y, PSTR("OFFON ")+3*g_model.extendedLimits,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event,g_model.extendedLimits,0,1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Trainer"),0);
    lcd_putsnAtt(  10*FW, y, PSTR("OFFON ")+3*g_model.traineron,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event,g_model.traineron,0,1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(0*FW, y, PSTR("DELETE MODEL   [MENU]"),s_noHi ? 0 : (sub==subN?INVERS:0));
    if(sub==subN && event==EVT_KEY_LONG(KEY_MENU)){
        s_editMode = false;
        s_noHi = NO_HI_LEN;
        killEvents(event);
				DupIfNonzero = 0 ;
        pushMenu(menuDeleteDupModel);
//        pushMenu(menuDeleteModel);

          //EFile::rm(FILE_MODEL(g_eeGeneral.currModel)); //delete file

          //uint8_t i = g_eeGeneral.currModel;//loop to find next available model
          //while (!EFile::exists(FILE_MODEL(i))) {
              //i--;
              //if(i>MAX_MODELS) i=MAX_MODELS-1;
              //if(i==g_eeGeneral.currModel) {
                  //i=0;
                  //break;
              //}
          //}
          //g_eeGeneral.currModel = i;

          //eeLoadModel(g_eeGeneral.currModel); //load default values
          //chainMenu(menuProcModelSelect);
    }
    if((y+=FH)>7*FH) return;
  }subN++;
}

void menuProcHeli(uint8_t event)
{
  static MState2 mstate2;
  TITLE("HELI SETUP ");
  MSTATE_TAB = { 1,1,1,1,1,1,1};
  MSTATE_CHECK_VxH(3,menuTabModel,7);
  int8_t  sub    = mstate2.m_posVert;

  if(sub<1) s_pgOfs=0;
  else if((sub-s_pgOfs)>7) s_pgOfs = sub-7;
  else if((sub-s_pgOfs)<1) s_pgOfs = sub-1;
  if(s_pgOfs<0) s_pgOfs = 0;

  uint8_t y = 1*FH;

  switch(event){
    case EVT_KEY_FIRST(KEY_MENU):
    case EVT_ENTRY:
      s_editMode = false;
      break;
  }

  uint8_t subN = 1;
  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Swash Type"),0);
    lcd_putsnAtt(  14*FW, y, PSTR(SWASH_TYPE_STR)+6*g_model.swashType,6,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event,g_model.swashType,0,SWASH_TYPE_NUM);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Collective"),0);
    putsChnRaw(14*FW, y, g_model.swashCollectiveSource,  sub==subN ? INVERS : 0);
    if(sub==subN) CHECK_INCDEC_H_MODELVAR(event, g_model.swashCollectiveSource, 0, NUM_XCHNRAW);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Swash Ring"),0);
    lcd_outdezAtt(14*FW+NUM_OFS(g_model.swashRingValue), y, g_model.swashRingValue,  sub==subN ? INVERS : 0);
    if(sub==subN) CHECK_INCDEC_H_MODELVAR(event, g_model.swashRingValue, 0, 100);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("ELE Direction"),0);
    lcd_putsnAtt(  14*FW, y, PSTR("---INV")+3*g_model.swashInvertELE,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event, g_model.swashInvertELE, 0, 1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("AIL Direction"),0);
    lcd_putsnAtt(  14*FW, y, PSTR("---INV")+3*g_model.swashInvertAIL,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event, g_model.swashInvertAIL, 0, 1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("COL Direction"),0);
    lcd_putsnAtt(  14*FW, y, PSTR("---INV")+3*g_model.swashInvertCOL,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_MODELVAR_BF(event, g_model.swashInvertCOL, 0, 1);
    if((y+=FH)>7*FH) return;
  }subN++;
}

void menuProcModelSelect(uint8_t event)
{
  static MState2 mstate2;
  TITLE("MODELSEL");
  lcd_puts_P(     9*FW, 0, PSTR("free"));
  lcd_outdezAtt(  17*FW, 0, EeFsGetFree(),0);

  lcd_putsAtt(128-FW*4,0,PSTR("1/10"),INVERS);

  int8_t subOld  = mstate2.m_posVert;
  MSTATE_CHECK0_V(MAX_MODELS);
  int8_t  sub    = mstate2.m_posVert;
  static uint8_t sel_editMode;
	if ( DupIfNonzero == 2 )
	{
		sel_editMode = false ;
		DupIfNonzero = 0 ;
	}
  switch(event)
  {
    //case  EVT_KEY_FIRST(KEY_MENU):
    case  EVT_KEY_FIRST(KEY_EXIT):
      if(sel_editMode){
        sel_editMode = false;
        beepKey();
        killEvents(event);
        eeLoadModel(g_eeGeneral.currModel = mstate2.m_posVert);
        resetTimer();
        STORE_GENERALVARS;
        STORE_MODELVARS;
        break;
      }
      //fallthrough
    case  EVT_KEY_FIRST(KEY_LEFT):
    case  EVT_KEY_FIRST(KEY_RIGHT):
      if(g_eeGeneral.currModel != mstate2.m_posVert)
      {
        killEvents(event);
        g_eeGeneral.currModel = mstate2.m_posVert;
        eeLoadModel(g_eeGeneral.currModel);
        resetTimer();
        STORE_GENERALVARS;
        beepWarn1();
      }
      if(event==EVT_KEY_FIRST(KEY_LEFT))  {killEvents(event);popMenu(true);}
      if(event==EVT_KEY_FIRST(KEY_RIGHT)) chainMenu(menuProcModel);
//      if(event==EVT_KEY_FIRST(KEY_EXIT))  chainMenu(menuProcModelSelect);
      break;
    case  EVT_KEY_FIRST(KEY_MENU):
        sel_editMode = true;
        beepKey();
        break;
    case  EVT_KEY_LONG(KEY_EXIT):  // make sure exit long exits to main
        popMenu(true);
        break;
    case  EVT_KEY_LONG(KEY_MENU):
      if(sel_editMode){
        
				DupIfNonzero = 1 ;
				DupSub = sub ;
      	pushMenu(menuDeleteDupModel);//menuProcExpoAll);
				
//        message(PSTR("Duplicating model"));
//        if(eeDuplicateModel(sub)) {
//          beepKey();
//          sel_editMode = false;
//        }
//        else beepWarn();
      }
      break;

    case EVT_ENTRY:
      sel_editMode = false;

      mstate2.m_posVert = g_eeGeneral.currModel;
      eeCheck(true); //force writing of current model data before this is changed
      break;
  }
  if(sel_editMode && subOld!=sub){
    EFile::swap(FILE_MODEL(subOld),FILE_MODEL(sub));
  }

  if(sub-s_pgOfs < 1)        s_pgOfs = max(0,sub-1);
  else if(sub-s_pgOfs >4 )  s_pgOfs = min(MAX_MODELS-6,sub-4);
  for(uint8_t i=0; i<6; i++){
    uint8_t y=(i+2)*FH;
    uint8_t k=i+s_pgOfs;
    lcd_outdezNAtt(  3*FW, y, k+1, ((sub==k) ? INVERS : 0) + LEADING0,2);
    static char buf[sizeof(g_model.name)+5];
    if(k==g_eeGeneral.currModel) lcd_putcAtt(1,  y,'*',0);
    eeLoadModelName(k,buf,sizeof(buf));
    lcd_putsnAtt(  4*FW, y, buf,sizeof(buf),BSS_NO_INV|((sub==k) ? (sel_editMode ? INVERS : 0 ) : 0));
  }

}



void menuProcDiagCalib(uint8_t event)
{
  static MState2 mstate2;
  TITLE("CALIBRATION");
  MSTATE_CHECK_V(6,menuTabDiag,4);
  int8_t  sub    = mstate2.m_posVert ;
  static int16_t midVals[7];
  static int16_t loVals[7];
  static int16_t hiVals[7];

  for(uint8_t i=0; i<7; i++) { //get low and high vals for sticks and trims
    int16_t vt = anaIn(i);
    loVals[i] = min(vt,loVals[i]);
    hiVals[i] = max(vt,hiVals[i]);
    //if(i>=4) midVals[i] = (loVals[i] + hiVals[i])/2;
  }

  switch(event)
  {
    case EVT_ENTRY:
      for(uint8_t i=0; i<7; i++) loVals[i] = 15000;
      break;
    case EVT_KEY_BREAK(KEY_DOWN): // !! achtung sub schon umgesetzt
      switch(sub)
      {
        case 2: //get mid
          for(uint8_t i=0; i<7; i++)midVals[i] = anaIn(i);
          beepKey();
          break;
        case 3:
          for(uint8_t i=0; i<7; i++)
            if(abs(loVals[i]-hiVals[i])>50) {
              g_eeGeneral.calibMid[i]  = midVals[i];
              int16_t v = midVals[i] - loVals[i];
              g_eeGeneral.calibSpanNeg[i] = v - v/64;
              v = hiVals[i] - midVals[i];
              g_eeGeneral.calibSpanPos[i] = v - v/64;
            }
          int16_t sum=0;
          for(uint8_t i=0; i<12;i++) sum+=g_eeGeneral.calibMid[i];
          g_eeGeneral.chkSum = sum;
          eeDirty(EE_GENERAL); //eeWriteGeneral();
          beepKey();
          break;
      }
      break;
  }
  for(uint8_t i=1; i<4; i++)
  {
    uint8_t y=i*FH+FH;
    lcd_putsnAtt( 0, y,PSTR("SetMid SetSpanDone   ")+7*(i-1),7,
                    sub==i ? INVERS : 0);
  }
  for(uint8_t i=0; i<7; i++)
  {
    uint8_t y=i*FH+0;
    lcd_puts_P( 11*FW,  y+1*FH, PSTR("<    >"));
    lcd_outhex4( 8*FW-3,y+1*FH, sub==2 ? loVals[i]  : g_eeGeneral.calibSpanNeg[i]);
    lcd_outhex4(12*FW,  y+1*FH, sub==1 ? anaIn(i) : (sub==2 ? midVals[i] : g_eeGeneral.calibMid[i]));
    lcd_outhex4(17*FW,  y+1*FH, sub==2 ? hiVals[i]  : g_eeGeneral.calibSpanPos[i]);
  }

}

void menuProcDiagAna(uint8_t event)
{
  static uint16_t count, deg=18;
  static MState2 mstate2;
  MSTATE_CHECK_V(1,menuTabDiag,1+5);

  if (count++ > 40) {
	  deg++;
	  count = 0;
  }
  if (deg >= 360) deg = 0;
	
  /* aircraft name */
  lcd_putsmAtt(1, 0,PSTR("FUNJET1"),0,0);
  /* flight time */
  lcd_putsmAtt(13*FW+1, 0,PSTR("00:01:42"),0,0);
  
  /* current block (name) */
  lcd_putsmAtt(1, 1*FH, PSTR("CircleStndby"),0,0);
  /* block time */
  lcd_putsmAtt(13*FW+1, 1*FH, PSTR("00:00:23"),0,0);
  
  /* speed */
  lcd_hlineStip(1, 2*FH+4, 8*FW,0xAA);
  lcd_putsmAtt(1*FW+1, 2*FH,PSTR("99.9ms"),0,0);
  /* throttle setting */
  lcd_hlineStip(13*FW+1, 2*FH+4, 8*FW,0xAA);
  lcd_putsmAtt(15*FW+1, 2*FH,PSTR("100%"),0,0);
  /* speed bar */
  lcd_barAtt(1, 2*FH, 21, INVERS);
  /* throttle setting bar */
  lcd_barAtt(13*FW, 2*FH, 21, INVERS);
  
  /* mode */
  lcd_putsmAtt(15*FW+1, 3*FH,PSTR("AUTO2"),0,0);
  
  /* climb rate */
//  lcd_putsmAtt(1+9*FW, 4*FH,PSTR("+10.2"),0,0);
  
  /* climb rate bar*/
  lcd_vline(8*FH,  18, 2*FW+2);
  lcd_vline(8*FH+1,17, 2*FW+3);
  lcd_vline(8*FH+2,18, 2*FW+2);
  
  /* link state */
//  lcd_putsmAtt(1+1*FW, 3*FH,PSTR("Lnk 999"),0,BLINK);
  
  /* rc state */
  lcd_putsmAtt(1+15*FW, 4*FH,PSTR("NO_RC"),0,INVERS);

  /* on board voltage */
  lcd_putsmAtt(5*FW, 4*FH,PSTR("."),0,DBLSIZE);
  lcd_putsmAtt(1*FW+1, 4*FH,PSTR("12"),0,DBLSIZE);
  lcd_putsmAtt(6*FW+1, 4*FH,PSTR("6"),0,DBLSIZE);
  
  /* gps state */
  lcd_putsmAtt(1+15*FW, 5*FH,PSTR("NOGPS"),0,INVERS);

  /* heading pointer */
  lcd_putsmAtt(1+6*FW, 6*FH,PSTR("V"),0,0);

#if 0  
  /* heading */
  lcd_hlineStip(1+1*FW, 7*FH+4, 11*FW,0x11);
  lcd_putsmAtt(1+1*FW, 7*FH,PSTR("180"),0,0);
  lcd_putsmAtt(1+9*FW, 7*FH,PSTR("270"),0,0);
#endif
  {
    int16_t quadr=deg/90, sect=((deg%90)*8+4)/15;

  lcd_outdez(1+10*FW, 4*FH, deg/100);
  lcd_outdez(1+11*FW, 4*FH, (deg/10)%10);
  lcd_outdez(1+12*FW, 4*FH, deg%10);

  lcd_outdez(1+10*FW, 5*FH, sect/100);
  lcd_outdez(1+11*FW, 5*FH, (sect/10)%10);
  lcd_outdez(1+12*FW, 5*FH, sect%10);

    /* heading */
    lcd_hlineStip(1+1*FW, 7*FH+4, 11*FW,0x88>>(sect%4));
    if (sect < 32) {
      lcd_putsnAtt(1+5*FW-sect, 7*FH, PSTR("000090180270")+3*quadr,3,0);
    } else
    if (sect < 38) {
      lcd_putsnAtt(1+6*FW-sect, 7*FH,PSTR("00908070")+3*quadr,2,0);
    } else
    if (sect < 44) {
      lcd_putsnAtt(1+7*FW-sect, 7*FH,PSTR("0"),1,0);
    }
    if (sect > 18) {
      lcd_putsnAtt(1+13*FW-sect, 7*FH,PSTR("090180270000")+3*quadr,3,0);
    } else
    if (sect > 12) {
      lcd_putsnAtt(1+13*FW-sect, 7*FH,PSTR("09182700")+3*quadr,2,0);
    } else
    if (sect > 6) {
      lcd_putsnAtt(1+13*FW-sect, 7*FH,PSTR("0120")+3*quadr,1,0);
    }
    
    
    
//    lcd_putsmAtt(1+1*FW+i, 7*FH,PSTR("000\t090\t180\t270"),0,0);
//    lcd_putsmAtt(1+9*FW+i, 7*FH,PSTR("090\t180\t270\t000"),0,0);
  }
    
  /* altitude */
  lcd_putsmAtt(1+13*FW, 6*FH,PSTR("A:1254m"),0,0);
  
  /* target altitude, clear heading border */
  lcd_putsmAtt(1, 7*FH,PSTR(" "),0,0);
  lcd_putsmAtt(1+12*FW, 7*FH,PSTR(" T:1260m"),0,0);

  /* on board voltage bar */
  lcd_vline(0*FH+1, 30, 34);
  lcd_vline(0*FH+2, 30, 34);
  lcd_vline(0*FH+3, 30, 34);
  lcd_vline(0*FH+4, 30, 34);
  lcd_vline(0*FH+5, 30, 34);
  
  /* altitude bar */
  lcd_vline(15*FH+2, 28, 43);
  lcd_vline(15*FH+3, 28, 43);
  lcd_vline(15*FH+4, 28, 43);
  lcd_vline(15*FH+5, 28, 43);
  lcd_vline(15*FH+6, 28, 43);
#if 0
  static MState2 mstate2;
  TITLE("ANA");
  MSTATE_CHECK_V(5,menuTabDiag,2);
  int8_t  sub    = mstate2.m_posVert ;

  for(uint8_t i=0; i<8; i++)
  {
    uint8_t y=i*FH;
    lcd_putsn_P( 4*FW, y,PSTR("A1A2A3A4A5A6A7A8")+2*i,2);
    lcd_outhex4( 8*FW, y,anaIn(i));
    if(i<7)  lcd_outdez(17*FW, y, (int32_t)calibratedStick[i]*100/1024);
    if(i==7) putsVBat(13*FW,y,false,(sub==1 ? INVERS : 0)|PREC1);
  }
  if(sub==1) CHECK_INCDEC_H_GENVAR(event, g_eeGeneral.vBatCalib, -127, 127);
#endif  
}

void menuProcDiagKeys(uint8_t event)
{
  static MState2 mstate2;
  TITLE("DIAG");
  MSTATE_CHECK_V(4,menuTabDiag,1);
  uint8_t x;

  x=7*FW;
  for(uint8_t i=0; i<9; i++)
  {
    uint8_t y=i*FH; //+FH;
    if(i>(SW_ID0-SW_BASE_DIAG)) y-=FH; //overwrite ID0
    bool t=keyState((EnumKeys)(SW_BASE_DIAG+i));
    putsDrSwitches(x,y,i+1,0); //ohne off,on
    lcd_putcAtt(x+FW*4+2,  y,t+'0',t ? INVERS : 0);
  }

  x=0;
  for(uint8_t i=0; i<6; i++)
  {
    uint8_t y=(5-i)*FH+2*FH;
    bool t=keyState((EnumKeys)(KEY_MENU+i));
    lcd_putsn_P(x, y,PSTR(" Menu Exit Down   UpRight Left")+5*i,5);
    lcd_putcAtt(x+FW*5+2,  y,t+'0',t);
  }

  x=14*FW;
  lcd_putsn_P(x, 3*FH,PSTR("Trim- +"),7);
  for(uint8_t i=0; i<4; i++)
  {
    uint8_t y=i*FH+FH*4;
    lcd_img(    x,       y, sticks,i,0);
    bool tm=keyState((EnumKeys)(TRM_BASE+2*i));
    bool tp=keyState((EnumKeys)(TRM_BASE+2*i+1));
    lcd_putcAtt(x+FW*4,  y, tm+'0',tm ? INVERS : 0);
    lcd_putcAtt(x+FW*6,  y, tp+'0',tp ? INVERS : 0);
  }
}

void menuProcDiagVers(uint8_t event)
{
  static MState2 mstate2;
  TITLE("VERSION");
  MSTATE_CHECK_V(3,menuTabDiag,1);
  lcd_puts_P(0, 2*FH,stamp4 );
  lcd_puts_P(0, 3*FH,stamp1 );
  lcd_puts_P(0, 4*FH,stamp2 );
  lcd_puts_P(0, 5*FH,stamp3 );
}

// From Bertrand, allow trainer inputs without using mixers.
// Raw trianer inputs replace raw sticks.
// Only first 4 PPMin may be calibrated.
void menuProcTrainer(uint8_t event)
{
  static MState2 mstate2;
  TITLE("TRAINER");
  MSTATE_TAB = {4,4};
  MSTATE_CHECK_VxH(2, menuTabDiag, 1+1+4+1);
  int8_t  sub    = mstate2.m_posVert;
  uint8_t subSub = mstate2.m_posHorz+1;
  uint8_t y;
  bool    edit;
	uint8_t blink ;

  if (SLAVE_MODE) { // i am the slave
    lcd_puts_P(7*FW, 3*FH, PSTR("Slave"));
    return;
  }

  switch(event)
  {
    case EVT_ENTRY:
      s_editMode = false;
      break;
    case EVT_KEY_FIRST(KEY_MENU):
      if (sub != 0 && sub != 6) s_editMode = !s_editMode;
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      if(s_editMode) {
        s_editMode = false;
        killEvents(event);
      }
      break;
  }

  lcd_puts_P(3*FW, 1*FH, PSTR("mode   % src  sw"));

  sub--;
  y = 2*FH;
	blink =	s_editMode ? BLINK : INVERS ;

  for (uint8_t i=0; i<4; i++) {
    volatile TrainerMix *td = &g_eeGeneral.trainer.mix[i];
    putsChnRaw(0, y, i+1, 0);

		edit = (sub==i && subSub==1);
    lcd_putsnAtt(4*FW, y, PSTR("off += :=")+3*td->mode, 3, edit ? blink : 0);
    if (edit && s_editMode)
      CHECK_INCDEC_H_GENVAR_BF(event, td->mode, 0, 2); //!! bitfield

    edit = (sub==i && subSub==2);
    lcd_outdezAtt(11*FW, y, td->studWeight*13/4, edit ? blink : 0);
    if (edit && s_editMode)
      CHECK_INCDEC_H_GENVAR_BF(event, td->studWeight, -31, 31); //!! bitfield

    edit = (sub==i && subSub==3);
    lcd_putsnAtt(12*FW, y, PSTR("ch1ch2ch3ch4")+3*td->srcChn, 3, edit ? blink : 0);
    if (edit && s_editMode)
      CHECK_INCDEC_H_GENVAR_BF(event, td->srcChn, 0, 3); //!! bitfield

    edit = (sub==i && subSub==4);
    putsDrSwitches(15*FW, y, td->swtch, edit ? blink : 0);
    if (edit && s_editMode)
      CHECK_INCDEC_H_GENVAR_BF(event, td->swtch, -MAX_DRSWITCH, MAX_DRSWITCH);

    y += FH;
	}

  lcd_putsAtt(0*FW, y, PSTR("Multiplier"), 0);
  lcd_outdezAtt(13*FW, y, g_eeGeneral.PPM_Multiplier+10, (sub==4 ? INVERS : 0)|PREC1);
  if(sub==4) CHECK_INCDEC_H_GENVAR(event, g_eeGeneral.PPM_Multiplier, -10, 40);
  y += FH;

  edit = (sub==5);
  lcd_putsAtt(0*FW, y, PSTR("Cal"), edit ? INVERS : 0);
  for (uint8_t i=0; i<4; i++) {
    uint8_t x = (i*8+16)*FW/2;
    lcd_outdezAtt(x , y, (g_ppmIns[i]-g_eeGeneral.trainer.calib[i])*2, PREC1);
  }
  if (edit) {
    if (event==EVT_KEY_FIRST(KEY_MENU)){
      memcpy(g_eeGeneral.trainer.calib, g_ppmIns, sizeof(g_eeGeneral.trainer.calib));
      eeDirty(EE_GENERAL);
      beepKey();
    }
  }
}

void menuProcSetup(uint8_t event)
{
#define COUNT_ITEMS 18
#define PARAM_OFS   17*FW
  static MState2 mstate2;
  TITLE("RADIO SETUP");
  MSTATE_CHECK_V(1,menuTabDiag,1+COUNT_ITEMS);
  int8_t  sub    = mstate2.m_posVert;
  uint8_t subSub = mstate2.m_posHorz + 1;

  if(sub<1) s_pgOfs=0;
  else if((sub-s_pgOfs)>7) s_pgOfs = sub-7;
  else if((sub-s_pgOfs)<1) s_pgOfs = sub-1;
  if(s_pgOfs<0) s_pgOfs = 0;

  if(s_pgOfs==COUNT_ITEMS-7) s_pgOfs= sub<(COUNT_ITEMS-4) ? COUNT_ITEMS-8 : COUNT_ITEMS-6;

  uint8_t y = 1*FH;
  uint8_t t = 0;


  switch(event){
    case EVT_ENTRY:
      s_editMode = false;
      break;
    case EVT_KEY_FIRST(KEY_MENU):
      if(sub>=0) s_editMode = !s_editMode;
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      if(s_editMode) {
        s_editMode = false;
        killEvents(event);
      }
      break;
    case EVT_KEY_REPT(KEY_LEFT):
    case EVT_KEY_FIRST(KEY_LEFT):
      if(sub==1 && subSub>1 && s_editMode) mstate2.m_posHorz--;
      break;
    case EVT_KEY_REPT(KEY_RIGHT):
    case EVT_KEY_FIRST(KEY_RIGHT):
      if(sub==1 && subSub<sizeof(g_model.name) && s_editMode) mstate2.m_posHorz++;
      break;
    case EVT_KEY_REPT(KEY_UP):
    case EVT_KEY_FIRST(KEY_UP):
    case EVT_KEY_REPT(KEY_DOWN):
    case EVT_KEY_FIRST(KEY_DOWN):
      if (!s_editMode) mstate2.m_posHorz = 0;
      break;
  }

  uint8_t subN = 1;

  if(s_pgOfs<subN) {
    lcd_putsAtt(    0,    y, PSTR("Owner Name"),0);
    lcd_putsnAtt(11*FW,   y, g_eeGeneral.ownerName ,sizeof(g_eeGeneral.ownerName),BSS_NO_INV | (sub==subN ? (s_editMode ? 0 : INVERS) : 0));
    if(sub==subN && s_editMode){
        char v = char2idx(g_eeGeneral.ownerName[subSub-1]);
        if(p1valdiff || event==EVT_KEY_FIRST(KEY_DOWN) || event==EVT_KEY_FIRST(KEY_UP) || event==EVT_KEY_REPT(KEY_DOWN) || event==EVT_KEY_REPT(KEY_UP))
           CHECK_INCDEC_H_GENVAR_BF( event,v ,0,NUMCHARS-1);
        v = idx2char(v);
        g_eeGeneral.ownerName[subSub-1]=v;
        lcd_putcAtt((11+subSub-1)*FW, y, v,INVERS);
    }
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Beeper"));
    lcd_putsnAtt(PARAM_OFS - FW, y, PSTR("Quiet""NoKey""Norm ""Long ""xLong")+5*g_eeGeneral.beeperVal,5,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_GENVAR_BF(event, g_eeGeneral.beeperVal, 0, 4);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Contrast"));
    lcd_outdezAtt(PARAM_OFS+NUM_OFS(g_eeGeneral.contrast),y,g_eeGeneral.contrast,sub==subN ? INVERS : 0);
    if(sub==subN) {
      CHECK_INCDEC_H_GENVAR(event, g_eeGeneral.contrast, 10, 45);
      lcdSetRefVolt(g_eeGeneral.contrast);
    }
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Battery warning"));
    t = PARAM_OFS + NUM_OFS(g_eeGeneral.vBatWarn);
    lcd_outdezAtt(t, y, g_eeGeneral.vBatWarn, (sub==subN ? INVERS : 0)|PREC1);
    lcd_putcAtt(  t, y, 'v', 0);
    if(sub==subN) CHECK_INCDEC_H_GENVAR(event, g_eeGeneral.vBatWarn, 40, 120); //5-10V
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Inactivity alarm"));
    t = PARAM_OFS + NUM_OFS(g_eeGeneral.inactivityTimer+10);
    lcd_outdezAtt(t, y, g_eeGeneral.inactivityTimer+10, (sub==subN ? INVERS : 0));
    lcd_putcAtt(  t, y, 'm', 0);
    if(sub==subN) CHECK_INCDEC_H_GENVAR(event, g_eeGeneral.inactivityTimer, -10, 110); //0..120minutes
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Filter ADC"));
    lcd_putsnAtt(PARAM_OFS, y, PSTR("SINGOSMPFILT")+4*g_eeGeneral.filterInput,4,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_GENVAR(event, g_eeGeneral.filterInput, 0, 2);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Throttle reverse"));
    lcd_putsnAtt(PARAM_OFS, y, PSTR("OFFON ")+3*g_eeGeneral.throttleReversed,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_GENVAR_BF(event, g_eeGeneral.throttleReversed, 0, 1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Minute beep"));
    lcd_putsnAtt(PARAM_OFS, y, PSTR("OFFON ")+3*g_eeGeneral.minuteBeep,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_GENVAR_BF(event, g_eeGeneral.minuteBeep, 0, 1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Beep countdown"));
    lcd_putsnAtt(PARAM_OFS, y, PSTR("OFFON ")+3*g_eeGeneral.preBeep,3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_GENVAR_BF(event, g_eeGeneral.preBeep, 0, 1);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
      lcd_puts_P(0, y,PSTR("Flash on beep"));
      lcd_putsnAtt(PARAM_OFS, y, PSTR("OFFON ")+3*g_eeGeneral.flashBeep,3,(sub==subN ? INVERS:0));
      if(sub==subN) CHECK_INCDEC_H_GENVAR_BF(event, g_eeGeneral.flashBeep, 0, 1);
      if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Light switch"));
    putsDrSwitches(PARAM_OFS-FW,y,g_eeGeneral.lightSw,sub==subN ? INVERS : 0);
    if(sub==subN) CHECK_INCDEC_H_GENVAR(event, g_eeGeneral.lightSw, -MAX_DRSWITCH, MAX_DRSWITCH);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
    lcd_puts_P(0, y,PSTR("Light off after"));
    if(g_eeGeneral.lightAutoOff)
    {
        uint8_t t = PARAM_OFS + NUM_OFS(g_eeGeneral.lightAutoOff*5);
        lcd_outdezAtt(t, y, g_eeGeneral.lightAutoOff*5,(sub==subN ? INVERS : 0));
        lcd_putcAtt(  t, y, 's', 0);

    }
    else
        lcd_putsnAtt(PARAM_OFS, y, PSTR("OFF"),3,(sub==subN ? INVERS:0));
    if(sub==subN) CHECK_INCDEC_H_GENVAR(event, g_eeGeneral.lightAutoOff, 0, 600/5);
    if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
      lcd_puts_P(0, y,PSTR("Splash screen"));
      lcd_putsnAtt(PARAM_OFS, y, PSTR("OFFON ")+3*(1-g_eeGeneral.disableSplashScreen),3,(sub==subN ? INVERS:0));
      if(sub==subN)
      {
          uint8_t b = 1-g_eeGeneral.disableSplashScreen;
          CHECK_INCDEC_H_GENVAR_BF(event, b, 0, 1);
          g_eeGeneral.disableSplashScreen = 1-b;
      }
      if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
      lcd_puts_P(0, y,PSTR("Throttle Warning"));
      lcd_putsnAtt(PARAM_OFS, y, PSTR("OFFON ")+3*(1-g_eeGeneral.disableThrottleWarning),3,(sub==subN ? INVERS:0));
      if(sub==subN)
      {
          uint8_t b = 1-g_eeGeneral.disableThrottleWarning;
          CHECK_INCDEC_H_GENVAR_BF(event, b, 0, 1);
          g_eeGeneral.disableThrottleWarning = 1-b;
      }
      if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
      lcd_puts_P(0, y,PSTR("Switch Warning"));
      lcd_putsnAtt(PARAM_OFS, y, PSTR("OFFON ")+3*(1-g_eeGeneral.disableSwitchWarning),3,(sub==subN ? INVERS:0));
      if(sub==subN)
      {
          uint8_t b = 1-g_eeGeneral.disableSwitchWarning;
          CHECK_INCDEC_H_GENVAR_BF(event, b, 0, 1);
          g_eeGeneral.disableSwitchWarning = 1-b;
      }
      if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
      lcd_puts_P(0, y,PSTR("Memory Warning"));
      lcd_putsnAtt(PARAM_OFS, y, PSTR("OFFON ")+3*(1-g_eeGeneral.disableMemoryWarning),3,(sub==subN ? INVERS:0));
      if(sub==subN)
      {
          uint8_t b = 1-g_eeGeneral.disableMemoryWarning;
          CHECK_INCDEC_H_GENVAR_BF(event, b, 0, 1);
          g_eeGeneral.disableMemoryWarning = 1-b;
      }
      if((y+=FH)>7*FH) return;
  }subN++;

  if(s_pgOfs<subN) {
      lcd_puts_P(0, y,PSTR("Alarm Warning"));
      lcd_putsnAtt(PARAM_OFS, y, PSTR("OFFON ")+3*(1-g_eeGeneral.disableAlarmWarning),3,(sub==subN ? INVERS:0));
      if(sub==subN)
      {
          uint8_t b = 1-g_eeGeneral.disableAlarmWarning;
          CHECK_INCDEC_H_GENVAR_BF(event, b, 0, 1);
          g_eeGeneral.disableAlarmWarning = 1-b;
      }
      if((y+=FH)>7*FH) return;
  }subN++;


  if(s_pgOfs<subN) {
    lcd_putsAtt( 1*FW, y, PSTR("Mode"),0);//sub==3?INVERS:0);
    if(y<7*FH) {for(uint8_t i=0; i<4; i++) lcd_img((6+4*i)*FW, y, sticks,i,0); }
    if((y+=FH)>7*FH) return;

    lcd_putcAtt( 3*FW, y, '1'+g_eeGeneral.stickMode,sub==subN?INVERS:0);
    for(uint8_t i=0; i<4; i++) putsChnRaw( (6+4*i)*FW, y,i+1,0);//sub==3?INVERS:0);

    if(sub==subN) CHECK_INCDEC_H_GENVAR(event,g_eeGeneral.stickMode,0,3);
    if((y+=FH)>7*FH) return;
  }subN++;

}

uint16_t s_timeCumTot;
uint16_t s_timeCumAbs;  //laufzeit in 1/16 sec
uint16_t s_timeCumSw;  //laufzeit in 1/16 sec
uint16_t s_timeCumThr;  //gewichtete laufzeit in 1/16 sec
uint16_t s_timeCum16ThrP; //gewichtete laufzeit in 1/16 sec
uint8_t  s_timerState;
#define TMR_OFF     0
#define TMR_RUNNING 1
#define TMR_BEEPING 2
#define TMR_STOPPED 3

int16_t  s_timerVal;
void timer(uint8_t val)
{
  int8_t tm = g_model.tmrMode;
  static uint16_t s_time;
  static uint16_t s_cnt;
  static uint16_t s_sum;
  static uint8_t sw_toggled;

  if(abs(tm)>=(TMR_VAROFS+MAX_DRSWITCH-1)){ //toggeled switch//abs(g_model.tmrMode)<(10+MAX_DRSWITCH-1)
    static uint8_t lastSwPos;
    if(!(sw_toggled | s_sum | s_cnt | s_time | lastSwPos)) lastSwPos = tm < 0;  // if initializing then init the lastSwPos
    uint8_t swPos = getSwitch(tm>0 ? tm-(TMR_VAROFS+MAX_DRSWITCH-1-1) : tm+(TMR_VAROFS+MAX_DRSWITCH-1-1) ,0);
    if(swPos && !lastSwPos)  sw_toggled = !sw_toggled;  //if switcdh is flipped first time -> change counter state
    lastSwPos = swPos;
  }

  s_cnt++;
  s_sum+=val;
  if(( get_tmr10ms()-s_time)<100) return; //1 sec
  s_time += 100;
  val     = s_sum/s_cnt;
  s_sum  -= val*s_cnt; //rest
  s_cnt   = 0;

  if(abs(tm)<TMR_VAROFS) sw_toggled = false; // not switch - sw timer off
  else if(abs(tm)<(TMR_VAROFS+MAX_DRSWITCH-1)) sw_toggled = getSwitch((tm>0 ? tm-(TMR_VAROFS-1) : tm+(TMR_VAROFS-1)) ,0); //normal switch

  s_timeCumTot               += 1;
  s_timeCumAbs               += 1;
  if(val) s_timeCumThr       += 1;
  if(sw_toggled) s_timeCumSw += 1;
  s_timeCum16ThrP            += val/2;

  s_timerVal = g_model.tmrVal;
  uint8_t tmrM = abs(g_model.tmrMode);
  if(tmrM==TMRMODE_NONE) s_timerState = TMR_OFF;
  else if(tmrM==TMRMODE_ABS) s_timerVal -= s_timeCumAbs;
  else if(tmrM<TMR_VAROFS) s_timerVal -= (tmrM&1) ? s_timeCum16ThrP/16 : s_timeCumThr;// stick% : stick
  else s_timerVal -= s_timeCumSw; //switch

  switch(s_timerState)
  {
    case TMR_OFF:
      if(g_model.tmrMode != TMRMODE_NONE) s_timerState=TMR_RUNNING;
      break;
    case TMR_RUNNING:
      if(s_timerVal<=0 && g_model.tmrVal) s_timerState=TMR_BEEPING;
      break;
    case TMR_BEEPING:
      if(s_timerVal <= -MAX_ALERT_TIME)   s_timerState=TMR_STOPPED;
      if(g_model.tmrVal == 0)             s_timerState=TMR_RUNNING;
      break;
    case TMR_STOPPED:
      break;
  }

  static int16_t last_tmr;

  if(last_tmr != s_timerVal)  //beep only if seconds advance
  {
      if(s_timerState==TMR_RUNNING)
      {
          if(g_eeGeneral.preBeep && g_model.tmrVal) // beep when 30, 15, 10, 5,4,3,2,1 seconds remaining
          {
              if(s_timerVal==30) {beepAgain=2; beepWarn2();} //beep three times
              if(s_timerVal==20) {beepAgain=1; beepWarn2();} //beep two times
              if(s_timerVal==10)  beepWarn2();
              if(s_timerVal<= 3)  beepWarn2();

              if(g_eeGeneral.flashBeep && (s_timerVal==30 || s_timerVal==20 || s_timerVal==10 || s_timerVal<=3))
                  g_LightOffCounter = FLASH_DURATION;
          }

          if(g_eeGeneral.minuteBeep && (((g_model.tmrDir ? g_model.tmrVal-s_timerVal : s_timerVal)%60)==0)) //short beep every minute
          {
              beepWarn2();
              if(g_eeGeneral.flashBeep) g_LightOffCounter = FLASH_DURATION;
          }
      }
      else if(s_timerState==TMR_BEEPING)
      {
          beepWarn();
          if(g_eeGeneral.flashBeep) g_LightOffCounter = FLASH_DURATION;
      }
  }
  last_tmr = s_timerVal;
  if(g_model.tmrDir) s_timerVal = g_model.tmrVal-s_timerVal; //if counting backwards - display backwards
}


#define MAXTRACE 120
uint8_t s_traceBuf[MAXTRACE];
uint16_t s_traceWr;
uint16_t s_traceCnt;
void trace()   // called in perOut - once envery 0.01sec
{
  //value for time described in g_model.tmrMode
  //OFFABSRUsRU%ELsEL%THsTH%ALsAL%P1P1%P2P2%P3P3%
  uint16_t v = 0;
  if((abs(g_model.tmrMode)>1) && (abs(g_model.tmrMode)<TMR_VAROFS)) {
    v = calibratedStick[CONVERT_MODE(abs(g_model.tmrMode)/2)-1];
    v = (g_model.tmrMode<0 ? RESX-v : v+RESX ) / (RESX/16);
  }
  timer(v);

  uint16_t val = calibratedStick[CONVERT_MODE(3)-1]; //Get throttle channel value
  val = (val+RESX) / (RESX/16); //calibrate it
  static uint16_t s_time;
  static uint16_t s_cnt;
  static uint16_t s_sum;
  s_cnt++;
  s_sum+=val;
  if(( get_tmr10ms()-s_time)<1000) //10 sec
    return;
  s_time= get_tmr10ms() ;
  val   = s_sum/s_cnt;
  s_sum = 0;
  s_cnt = 0;

  s_traceCnt++;
  s_traceBuf[s_traceWr++] = val;
  if(s_traceWr>=MAXTRACE) s_traceWr=0;
}


uint16_t g_tmr1Latency_max;
uint16_t g_tmr1Latency_min = 0x7ff;
uint16_t g_timeMain;
void menuProcStatistic2(uint8_t event)
{
  TITLE("STAT2");
  switch(event)
  {
    case EVT_KEY_FIRST(KEY_MENU):
      g_tmr1Latency_min = 0x7ff;
      g_tmr1Latency_max = 0;
      g_timeMain    = 0;
      beepKey();
      break;
    case EVT_KEY_FIRST(KEY_DOWN):
      chainMenu(menuProcStatistic);
      break;
    case EVT_KEY_FIRST(KEY_UP):
    case EVT_KEY_FIRST(KEY_EXIT):
      chainMenu(menuProc0);
      break;
  }
  lcd_puts_P( 0*FW,  1*FH, PSTR("tmr1Lat max    us"));
  lcd_outdez(14*FW , 1*FH, g_tmr1Latency_max/2 );
  lcd_puts_P( 0*FW,  2*FH, PSTR("tmr1Lat min    us"));
  lcd_outdez(14*FW , 2*FH, g_tmr1Latency_min/2 );
  lcd_puts_P( 0*FW,  3*FH, PSTR("tmr1 Jitter    us"));
  lcd_outdez(14*FW , 3*FH, (g_tmr1Latency_max - g_tmr1Latency_min) /2 );
  lcd_puts_P( 0*FW,  4*FH, PSTR("tmain          ms"));
  lcd_outdezAtt(14*FW , 4*FH, (g_timeMain*100)/16 ,PREC2);
  lcd_puts_P( 3*FW,  7*FH, PSTR("[MENU] to refresh"));
}

#ifdef JETI

void menuProcJeti(uint8_t event)
{
  TITLE("JETI");

  switch(event)
  {
    //case EVT_KEY_FIRST(KEY_MENU):
    //  break;
    case EVT_KEY_FIRST(KEY_EXIT):
      JETI_DisableRXD();
      chainMenu(menuProc0);
      break;
  }

  for (uint8_t i = 0; i < 16; i++)
  {
    lcd_putcAtt((i+2)*FW,   3*FH, JetiBuffer[i], BSS_NO_INV);
    lcd_putcAtt((i+2)*FW,   4*FH, JetiBuffer[i+16], BSS_NO_INV);
  }

  if (JetiBufferReady)
  {
    JETI_EnableTXD();
    if (keyState((EnumKeys)(KEY_UP))) jeti_keys &= JETI_KEY_UP;
    if (keyState((EnumKeys)(KEY_DOWN))) jeti_keys &= JETI_KEY_DOWN;
    if (keyState((EnumKeys)(KEY_LEFT))) jeti_keys &= JETI_KEY_LEFT;
    if (keyState((EnumKeys)(KEY_RIGHT))) jeti_keys &= JETI_KEY_RIGHT;

    JetiBufferReady = 0;    // invalidate buffer

    JETI_putw((uint16_t) jeti_keys);
    _delay_ms (1);
    JETI_DisableTXD();

    jeti_keys = JETI_KEY_NOCHANGE;
  }
}
#endif

#ifdef FRSKY

void menuProcFrsky(uint8_t event);
void menuProcFrsky1(uint8_t event);
void menuProcFrskyConfig(uint8_t event);

uint8_t requestStatus = 1;

uint8_t hex2dec(uint8_t number, uint8_t multiplier)
{
  uint8_t value = 0;

  switch (multiplier)
  {
    case 1:
      value = number%100;
      value = value%10;
      break;

    case 10:
      value = number%100;
      value = value /10;
      break;

    case 100:
      value = number/100;
      break;

    default:

      break;
  }

  value += 48;
  return value;

}

void menuProcFrsky(uint8_t event)
{
  TITLE("FrSky        Page 1/3");

  switch(event)
  {    
	case EVT_KEY_FIRST(KEY_DOWN):
       chainMenu(menuProcFrsky1);
       break;
    case EVT_KEY_FIRST(KEY_EXIT):
      FRSKY_DisableRXD();
      chainMenu(menuProc0);
      break;
  }

  if (FrskyBufferReady)
  {
    uint8_t i=0;
	uint8_t voltAdjust;
	
	/* 'universal' voltage sensor cable uses divide ratio of 7.67:1 
	    (or 0.13). V2 rx uses 1:4 or 0.25. Ratios below are multiplied 
		by 100 so 0.13 = 13 etc.  To display the correct value for 
		other ratios, Display = (13/voltRatio) * ADC value.
		If ratio is 13 then obviously display = ADC			      	*/
		
	
	if (g_model.a1voltRatio == 130)
		voltAdjust = linkBuffer[0];
	else 
		voltAdjust = (130*linkBuffer[0])/g_model.a1voltRatio;
		
	
	
	linkBuffer[3] /= 2;		// Tx RSSI value is doubled
    
    TelemBuffer[3] = hex2dec(voltAdjust, 100);
    TelemBuffer[4] = hex2dec(voltAdjust, 10);
    TelemBuffer[6] = hex2dec(voltAdjust, 1);
    i++;
        
    i++;
    
    TelemBuffer[25] = hex2dec(linkBuffer[i], 100);
    TelemBuffer[26] = hex2dec(linkBuffer[i], 10);
    TelemBuffer[27] = hex2dec(linkBuffer[i], 1);
	i++;
	
    
    FrskyBufferReady = 0;
  }
  
  lcd_puts_P(  1*FW, FH*1, PSTR(" Pack Volts"));    
  lcd_puts_P(  1*FW, FH*4,PSTR(" Rx RSSI") );

  for (uint8_t i = 3; i < 8; i++)
  {
  lcd_putcAtt((i-2)*FW*2,   2*FH, TelemBuffer[i], DBLSIZE);
  lcd_putcAtt((i-2)*FW*2,   5*FH, TelemBuffer[i+22], DBLSIZE);
  }

}

void menuProcFrsky1(uint8_t event)
{
  TITLE("FrSky        Page 2/3");
  
  switch(event)
  {    
	case EVT_KEY_FIRST(KEY_UP):
	  chainMenu(menuProcFrsky);
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      FRSKY_DisableRXD();
      chainMenu(menuProc0);
      break;
	case EVT_KEY_FIRST(KEY_DOWN):
       chainMenu(menuProcFrskyConfig);
       break;
	case EVT_KEY_FIRST(KEY_RIGHT):
	   displayState = !displayState;
  }

  
  
  if (FrskyBufferReady)
  {
    uint8_t i=0;
	uint8_t voltAdjust;
	
	if (g_model.a2voltRatio == 130)
		voltAdjust = linkBuffer[1];
	else 
		voltAdjust = (130*linkBuffer[1])/g_model.a2voltRatio;
		
	linkBuffer[3] /= 2;		// Tx RSSI value is doubled
        
    i++;
    
    TelemBuffer[11] = hex2dec(voltAdjust, 100);
    TelemBuffer[12] = hex2dec(voltAdjust, 10);
    TelemBuffer[14] = hex2dec(voltAdjust, 1);
    i++;
        
	i++;
	
    TelemBuffer[38] = hex2dec(linkBuffer[i], 100);
    TelemBuffer[39] = hex2dec(linkBuffer[i], 10);
    TelemBuffer[40] = hex2dec(linkBuffer[i], 1);
    FrskyBufferReady = 0;
  }
  
  lcd_puts_P(  1*FW, FH*1, PSTR(" Analogue 2"));    
  lcd_puts_P(  1*FW, FH*4,PSTR(" Tx RSSI") );

  for (uint8_t i = 3; i < 8; i++)
  {
    if (displayState) {
		lcd_putcAtt((i-2)*FW*2,   2*FH, TelemBuffer[i+8], DBLSIZE);
	}
	else {
		if (i>=5)
			lcd_putcAtt((i-2)*FW*2,   2*FH, TelemBuffer[i+9], DBLSIZE);
		else 
			lcd_putcAtt((i-2)*FW*2,   2*FH, TelemBuffer[i+8], DBLSIZE);
	}
	//lcd_putcAtt((i-2)*FW*2,   2*FH, TelemBuffer[i+8], DBLSIZE);
    lcd_putcAtt((i-2)*FW*2,   5*FH, TelemBuffer[i+35], DBLSIZE);
  }

}

void menuProcFrskyConfig(uint8_t event)
{
  
  static uint8_t editCnt = 1;
  uint8_t i;
  uint16_t bigNum;
   
  TITLE("FrSky Config Page 3/3");

  switch(event)
  {
	case EVT_KEY_FIRST(KEY_MENU):
		if (fr_editMode && editCnt == 15) {
	  
			// transmit strings code goes here
			FRSKY_saveAlarms();
			STORE_MODELVARS;
		  
			editCnt = 1;			  
		}
		fr_editMode = !fr_editMode;	
	  	  
		break;
	case EVT_KEY_FIRST(KEY_UP):
       if (fr_editMode) {
			if (editCnt > 1)
				editCnt--;		
	   }
	   else {
		  	FRSKY_DisableTXD();
			chainMenu(menuProcFrsky1);		
	   }
	   break;
	case EVT_KEY_FIRST(KEY_DOWN):
       if (fr_editMode && editCnt < 15) {
			editCnt++;
	   }
       break;
    case EVT_KEY_FIRST(KEY_EXIT):
      FRSKY_DisableRXD();
	  FRSKY_DisableTXD();
      chainMenu(menuProc0);
	  if(fr_editMode) {
		fr_editMode = false;
	  }
	  requestStatus = 2;
      break;	
  }  
  
  FRSKY_EnableTXD();
     
  if (requestStatus) {
    for (uint8_t k=0;k<11;k++) {			// setup loop to transmit string
	FRSKY_Transmit(alrmRequest[k]);		    // output character
    } 										// next character	
	requestStatus--;
  }
  
  if (fr_editMode) {
	  alrmPktRx |= 0x0F;
  }
  
/* 'universal' voltage sensor cable uses divide ratio of 7.67:1 
	(or 0.13). V2 rx uses 1:4 or 0.25. Ratios below are multiplied 
	by 100 so 0.13 = 13 etc.  To save the correct value for 
	alarm settings (a11 and a12) for other ratios, 
	Display = (voltRatio/13) * alarm value.
	If ratio is 13 then obviously display = ADC			      	*/   

  if (g_model.a1voltRatio == 130) {
		a11Adjust = a11Buffer[0];
		a12Adjust = a12Buffer[0];
  }
  else {
		a11Adjust = (130*a11Buffer[0])/g_model.a1voltRatio;
		
		a12Adjust = (130*a12Buffer[0])/g_model.a1voltRatio;
  }
  
  if (g_model.a2voltRatio == 130) {
		a21Adjust = a21Buffer[0];
		a22Adjust = a22Buffer[0];
  }
  else {
		a21Adjust = (130*a21Buffer[0])/g_model.a2voltRatio;
		
		a22Adjust = (130*a22Buffer[0])/g_model.a2voltRatio;
  }
  
  if (fr_editMode && editCnt == 1) {
  	lcd_outdezAtt(17*FW, 1*FH, g_model.a1voltRatio,INVERS);
	bigNum = g_model.a1voltRatio;
	CHECK_INCDEC_H_GENVAR(event, bigNum, 0, 254);
	g_model.a1voltRatio = bigNum;
  }
  else
	lcd_outdezAtt(17*FW, 1*FH, g_model.a1voltRatio,0);
  //i++;
 
  if(alrmPktRx & 1)
  {
	i=0;
	
	
	
	
    if (fr_editMode && editCnt == 3) {
		lcd_outdezAtt(16*FW, 2*FH, a11Adjust,INVERS);
		bigNum = a11Buffer[i];
		CHECK_INCDEC_H_GENVAR(event, bigNum, 0, 254);
		a11Buffer[i] = bigNum;
	}
	else
		lcd_outdezAtt(16*FW, 2*FH, a11Adjust,0);
	
	i++;
  
	
	if (fr_editMode && editCnt == 2) {
		lcd_putsnAtt(12*FW, 2*FH, PSTR("<"">")+1*a11Buffer[i],1,INVERS);
		CHECK_INCDEC_H_GENVAR(event, a11Buffer[i], 0, 1);
	}
	else
		lcd_putsnAtt(12*FW, 2*FH, PSTR("<"">")+1*a11Buffer[i],1,0);
	i++;
  
	
	if (fr_editMode && editCnt == 4) {
		lcd_putsnAtt(17*FW, 2*FH, PSTR("Off""Yel""Org""Red")+3*a11Buffer[i],3,INVERS);
		CHECK_INCDEC_H_GENVAR(event, a11Buffer[i], 0, 3);
	}
	else
		lcd_putsnAtt(17*FW, 2*FH, PSTR("Off""Yel""Org""Red")+3*a11Buffer[i],3,0);
	
  }

  if(alrmPktRx & 2)
  {
	i=0;
	
	
    if (fr_editMode && editCnt == 6) {
		lcd_outdezAtt(16*FW, 3*FH, a12Adjust,INVERS);
		bigNum = a12Buffer[i];
		CHECK_INCDEC_H_GENVAR(event, bigNum, 0, 254);
		a12Buffer[i] = bigNum;
	}
	else
		lcd_outdezAtt(16*FW, 3*FH, a12Adjust,0);
	
	i++;
  
	
	if (fr_editMode && editCnt == 5) {
		lcd_putsnAtt(12*FW, 3*FH, PSTR("<"">")+1*a12Buffer[i],1,INVERS);
		CHECK_INCDEC_H_GENVAR(event, a12Buffer[i], 0, 1);
	}
	else
		lcd_putsnAtt(12*FW, 3*FH, PSTR("<"">")+1*a12Buffer[i],1,0);
	i++;
  
	
	if (fr_editMode && editCnt == 7) {
		lcd_putsnAtt(17*FW, 3*FH, PSTR("Off""Yel""Org""Red")+3*a12Buffer[i],3,INVERS);
		CHECK_INCDEC_H_GENVAR(event, a12Buffer[i], 0, 3);
	}
	else
		lcd_putsnAtt(17*FW, 3*FH, PSTR("Off""Yel""Org""Red")+3*a12Buffer[i],3,0);
	
  }
  
  
  
  if (fr_editMode && editCnt == 8) {
  	lcd_outdezAtt(17*FW, 4*FH, g_model.a2voltRatio,INVERS);
	bigNum = g_model.a2voltRatio;
	CHECK_INCDEC_H_GENVAR(event, bigNum, 0, 254);
	g_model.a2voltRatio = bigNum;
  }
  else
	lcd_outdezAtt(17*FW, 4*FH, g_model.a2voltRatio,0);
  //i++;
  
  if(alrmPktRx & 4)
  {
	i=0;
	
	
    if (fr_editMode && editCnt == 10) {
		lcd_outdezAtt(16*FW, 5*FH, a21Adjust,INVERS);
		bigNum = a21Buffer[i];
		CHECK_INCDEC_H_GENVAR(event, bigNum, 0, 254);
		a21Buffer[i] = bigNum;
	}
	else
		lcd_outdezAtt(16*FW, 5*FH, a21Adjust,0);
	
	i++;
  
	
	if (fr_editMode && editCnt == 9) {
		lcd_putsnAtt(12*FW, 5*FH, PSTR("<"">")+1*a21Buffer[i],1,INVERS);
		CHECK_INCDEC_H_GENVAR(event, a21Buffer[i], 0, 1);
	}
	else
		lcd_putsnAtt(12*FW, 5*FH, PSTR("<"">")+1*a21Buffer[i],1,0);
	i++;
  
	
	if (fr_editMode && editCnt == 11) {
		lcd_putsnAtt(17*FW, 5*FH, PSTR("Off""Yel""Org""Red")+3*a21Buffer[i],3,INVERS);
		CHECK_INCDEC_H_GENVAR(event, a21Buffer[i], 0, 3);
	}
	else
		lcd_putsnAtt(17*FW, 5*FH, PSTR("Off""Yel""Org""Red")+3*a21Buffer[i],3,0);
	
  }
  
  if(alrmPktRx & 8)
  {
	i=0;
	
	
    if (fr_editMode && editCnt == 13) {
		lcd_outdezAtt(16*FW, 6*FH, a22Adjust,INVERS);
		bigNum = a22Buffer[i];
		CHECK_INCDEC_H_GENVAR(event, bigNum, 0, 254);
		a22Buffer[i] = bigNum;
	}
	else
		lcd_outdezAtt(16*FW, 6*FH, a22Adjust,0);
	
	i++;
  
	
	if (fr_editMode && editCnt == 12) {
		lcd_putsnAtt(12*FW, 6*FH, PSTR("<"">")+1*a22Buffer[i],1,INVERS);
		CHECK_INCDEC_H_GENVAR(event, a22Buffer[i], 0, 1);
	}
	else
		lcd_putsnAtt(12*FW, 6*FH, PSTR("<"">")+1*a22Buffer[i],1,0);
	i++;
  
	
	if (fr_editMode && editCnt == 14) {
		lcd_putsnAtt(17*FW, 6*FH, PSTR("Off""Yel""Org""Red")+3*a22Buffer[i],3,INVERS);
		CHECK_INCDEC_H_GENVAR(event, a22Buffer[i], 0, 3);
	}
	else
		lcd_putsnAtt(17*FW, 6*FH, PSTR("Off""Yel""Org""Red")+3*a22Buffer[i],3,0);
	
  }
  
  
  
  
  if (fr_editMode) {
		if (editCnt == 15)
			lcd_putsAtt(17*FW, 7*FH, PSTR("SAVE"),INVERS);	   
	    else		  	
			lcd_putsAtt(17*FW, 7*FH, PSTR("SAVE"),0);
  }
  
  
	
  
	lcd_puts_P(  1*FW, FH*2, PSTR("Alarm 1:1:"));  
	lcd_puts_P(  1*FW, FH*3, PSTR("Alarm 1:2:"));
	lcd_puts_P(  1*FW, FH*1, PSTR("a1 Cal: 1000:"));
	lcd_puts_P(  1*FW, FH*5, PSTR("Alarm 2:1:"));
	lcd_puts_P(  1*FW, FH*6, PSTR("Alarm 2:2:"));
	lcd_puts_P(  1*FW, FH*4, PSTR("a2 Cal: 1000:"));
  
}  
#endif

#ifdef PAPARAZZI

void menuProcJeti(uint8_t event)
{
  TITLE("PAPARAZZI");

  switch(event)
  {
    //case EVT_KEY_FIRST(KEY_MENU):
    //  break;
    case EVT_KEY_FIRST(KEY_EXIT):
      JETI_DisableRXD();
      chainMenu(menuProc0);
      break;
  }

  for (uint8_t i = 0; i < 16; i++)
  {
    lcd_putcAtt((i+2)*FW,   3*FH, JetiBuffer[i], BSS_NO_INV);
    lcd_putcAtt((i+2)*FW,   4*FH, JetiBuffer[i+16], BSS_NO_INV);
  }

  if (JetiBufferReady)
  {
    JETI_EnableTXD();
    if (keyState((EnumKeys)(KEY_UP))) jeti_keys &= JETI_KEY_UP;
    if (keyState((EnumKeys)(KEY_DOWN))) jeti_keys &= JETI_KEY_DOWN;
    if (keyState((EnumKeys)(KEY_LEFT))) jeti_keys &= JETI_KEY_LEFT;
    if (keyState((EnumKeys)(KEY_RIGHT))) jeti_keys &= JETI_KEY_RIGHT;

    JetiBufferReady = 0;    // invalidate buffer

    JETI_putw((uint16_t) jeti_keys);
    _delay_ms (1);
    JETI_DisableTXD();

    jeti_keys = JETI_KEY_NOCHANGE;
  }
}
#endif

void menuProcStatistic(uint8_t event)
{
  TITLE("STAT");
  switch(event)
  {
    case EVT_KEY_FIRST(KEY_UP):
      chainMenu(menuProcStatistic2);
      break;
    case EVT_KEY_FIRST(KEY_DOWN):
    case EVT_KEY_FIRST(KEY_EXIT):
      chainMenu(menuProc0);
      break;
  }

  lcd_puts_P(  1*FW, FH*1, PSTR("TME"));
  putsTime(    4*FW, FH*1, s_timeCumAbs, 0, 0);
  lcd_puts_P( 17*FW, FH*1, PSTR("TSW"));
  putsTime(   10*FW, FH*1, s_timeCumSw,      0, 0);

  lcd_puts_P(  1*FW, FH*2, PSTR("STK"));
  putsTime(    4*FW, FH*2, s_timeCumThr, 0, 0);
  lcd_puts_P( 17*FW, FH*2, PSTR("ST%"));
  putsTime(   10*FW, FH*2, s_timeCum16ThrP/16, 0, 0);

  lcd_puts_P( 17*FW, FH*0, PSTR("TOT"));
  putsTime(   10*FW, FH*0, s_timeCumTot, 0, 0);

  uint16_t traceRd = s_traceCnt>MAXTRACE ? s_traceWr : 0;
  uint8_t x=5;
  uint8_t y=60;
  lcd_hline(x-3,y,120+3+3);
  lcd_vline(x,y-32,32+3);

  for(uint8_t i=0; i<120; i+=6)
  {
    lcd_vline(x+i+6,y-1,3);
  }
  for(uint8_t i=1; i<=120; i++)
  {
    lcd_vline(x+i,y-s_traceBuf[traceRd],s_traceBuf[traceRd]);
    traceRd++;
    if(traceRd>=MAXTRACE) traceRd=0;
    if(traceRd==s_traceWr) break;
  }

}

//extern volatile uint16_t captureRing[16];

void resetTimer()
{
    s_timerState = TMR_OFF; //is changed to RUNNING dep from mode
    s_timeCumAbs=0;
    s_timeCumThr=0;
    s_timeCumSw=0;
    s_timeCum16ThrP=0;
    beepKey();
}

void menuProc0(uint8_t event)
{
  static uint8_t   sub;
  static MenuFuncP s_lastPopMenu[2];
  static uint8_t trimSwLock;

  switch(event)
  {
    case  EVT_KEY_LONG(KEY_MENU):// go to last menu
      //pushMenu(lastMenu);
      pushMenu(lastPopMenu());
      killEvents(event);
      break;
    case EVT_KEY_FIRST(KEY_RIGHT):
      if(getEventDbl(event)==2 && s_lastPopMenu[1]){
        pushMenu(s_lastPopMenu[1]);
        break;
      }
      if(sub<1) {
        sub=sub+1;
        beepKey();
      }
      break;
    case EVT_KEY_LONG(KEY_RIGHT):
      pushMenu(menuProcModelSelect);//menuProcExpoAll);
      killEvents(event);
      break;
    case EVT_KEY_FIRST(KEY_LEFT):
      if(getEventDbl(event)==2 && s_lastPopMenu[0]){
        pushMenu(s_lastPopMenu[0]);
        break;
      }
      if(sub>0) {
        sub=sub-1;
        beepKey();
      }
      break;
    case EVT_KEY_LONG(KEY_LEFT):
      pushMenu(menuProcSetup);
      killEvents(event);
      break;
#define MAX_VIEWS 5
    case EVT_KEY_BREAK(KEY_UP):
      g_eeGeneral.view++;
      if(g_eeGeneral.view>=MAX_VIEWS) g_eeGeneral.view=0;
      eeDirty(EE_GENERAL);
      beepKey();
      break;
    case EVT_KEY_BREAK(KEY_DOWN):
      if(g_eeGeneral.view>0)
        g_eeGeneral.view--;
      else
        g_eeGeneral.view = MAX_VIEWS-1;
      eeDirty(EE_GENERAL);
      beepKey();
      break;
    case EVT_KEY_LONG(KEY_UP):
      chainMenu(menuProcStatistic);
      killEvents(event);
      break;
    case EVT_KEY_LONG(KEY_DOWN):
#if (!(defined(JETI) || defined(FRSKY) || defined(ARDUPILOT) || defined(PAPARAZZI)))
      chainMenu(menuProcStatistic2);
#endif
#ifdef JETI
      JETI_EnableRXD(); // enable JETI-Telemetry reception
      chainMenu(menuProcJeti);
#endif
#ifdef FRSKY
      FRSKY_EnableRXD(); // enable FrSky-Telemetry reception
      chainMenu(menuProcFrsky);
#endif
#ifdef ARDUPILOT
      ARDUPILOT_EnableRXD(); // enable ArduPilot-Telemetry reception
      chainMenu(menuProcArduPilot);
#endif
#ifdef PAPARAZZI
      JETI_EnableRXD(); // enable JETI-Telemetry reception
      chainMenu(menuProcJeti);
#endif
      killEvents(event);
      break;
    case EVT_KEY_FIRST(KEY_EXIT):
      if(s_timerState==TMR_BEEPING) {
        s_timerState = TMR_STOPPED;
        beepKey();
      }
      break;
    case EVT_KEY_LONG(KEY_EXIT):
      resetTimer();
      break;
    case EVT_ENTRY_UP:
      s_lastPopMenu[sub] = lastPopMenu();
    case EVT_ENTRY:
      killEvents(KEY_EXIT);
      killEvents(KEY_UP);
      killEvents(KEY_DOWN);
      trimSwLock = true;
      break;
  }


  if(getSwitch(g_model.trimSw,0) && !trimSwLock) setStickCenter();
  trimSwLock = getSwitch(g_model.trimSw,0);

  uint8_t x=FW*2;
    uint8_t att = (g_vbat100mV < g_eeGeneral.vBatWarn ? BLINK : 0) | DBLSIZE;
    for(uint8_t i=0;i<sizeof(g_model.name);i++)
      lcd_putcAtt(x+i*2*FW-i-2, 0*FH, g_model.name[i],DBLSIZE);

    putsVBat(x-1*FW,2*FH,true, att);
    lcd_putcAtt(x+4*FW, 3*FH, 'V',0);

    uint8_t ln = 2;
    uint8_t xn = x;
    uint8_t tn = (g_vbat100mV/10) % 10;
    uint8_t sn = g_vbat100mV % 10;

    if(sn==2 || sn==3 || sn==1) ln++;
    if(tn==2 || tn==4) {xn--;ln++;}

    lcd_hline(xn+2*FW,4*FH-4,ln);
    lcd_hline(xn+2*FW,4*FH-3,ln);


  if(s_timerState != TMR_OFF){
    uint8_t att = DBLSIZE | (s_timerState==TMR_BEEPING ? BLINK : 0);
    putsTime(x+9*FW, FH*2, s_timerVal, att,att);
    putsTmrMode(x+7*FW-FW/2,FH*3,0);
  }

  lcd_putsnAtt(x+4*FW,     2*FH,PSTR("ExpExFFneMedCrs")+3*g_model.trimInc,3, 0);
  lcd_putsnAtt(x+8*FW-FW/2,2*FH,PSTR("   TTm")+3*g_model.thrTrim,3, 0);

  //trim sliders
  for(uint8_t i=0; i<4; i++)
  {
#define TL 27
    //                        LH LV RV RH
    static uint8_t x[4]    = {128*1/4+2, 4, 128-4, 128*3/4-2};
    static uint8_t vert[4] = {0,1,1,0};
    uint8_t xm,ym;
    xm=x[i];
    int8_t val = max((int8_t)-(TL+1),min((int8_t)(TL+1),(int8_t)(g_model.trim[i]/4)));
    if(vert[i]){
      ym=31;
      lcd_vline(xm,   ym-TL, TL*2);

      if(((g_eeGeneral.stickMode&1) != (i&1)) || !(g_model.thrTrim)){
        lcd_vline(xm-1, ym-1,  3);
        lcd_vline(xm+1, ym-1,  3);
      }
      ym -= val;
    }else{
      ym=59;
      lcd_hline(xm-TL,ym,    TL*2);
      lcd_hline(xm-1, ym-1,  3);
      lcd_hline(xm-1, ym+1,  3);
      xm += val;
    }
    DO_SQUARE(xm,ym,7)
  }

  if(g_eeGeneral.view<2) {
   for(uint8_t i=0; i<8; i++)
   {
    uint8_t x0,y0;
    int16_t val = g_chans512[i];
    //val += g_model.limitData[i].revert ? g_model.limitData[i].offset : -g_model.limitData[i].offset;
    switch(g_eeGeneral.view)
    {
      case 0:
        x0 = (i%4*9+3)*FW/2;
        y0 = i/4*FH+40;
        // *1000/1024 = x - x/8 + x/32
#define GPERC(x)  (x - x/32 + x/128)
        lcd_outdezAtt( x0+4*FW , y0, GPERC(val),PREC1 );
        break;
      case 1:
#define WBAR2 (50/2)
#define GPERC2(x) GPERC(x/2)
        x0       = i<4 ? 128/4+2 : 128*3/4-2;
        y0       = 38+(i%4)*5;
        int8_t l = (abs(GPERC2(val))+WBAR2/2) * WBAR2 / 512;
        if(l>WBAR2)  l =  WBAR2;  // prevent bars from going over the end - comment for debugging

        lcd_hlineStip(x0-WBAR2,y0,WBAR2*2+1,0x55);
        lcd_vline(x0,y0-2,5);
        if(g_chans512[i]>0){
          x0+=1;
        }else{
          x0-=l;
        }
        lcd_hline(x0,y0+1,l);
        lcd_hline(x0,y0-1,l);
        break;
    }
   }
  }
  else {
    #define BOX_WIDTH     23
    #define BAR_HEIGHT    (BOX_WIDTH-1l)
    #define MARKER_WIDTH  5
    #define SCREEN_WIDTH  128
    #define SCREEN_HEIGHT 64
    #define BOX_LIMIT     (BOX_WIDTH-MARKER_WIDTH)
    #define LBOX_CENTERX  (  SCREEN_WIDTH/4 + 10)
    #define LBOX_CENTERY  (SCREEN_HEIGHT-9-BOX_WIDTH/2)
    #define RBOX_CENTERX  (3*SCREEN_WIDTH/4 - 10)
    #define RBOX_CENTERY  (SCREEN_HEIGHT-9-BOX_WIDTH/2)

    DO_SQUARE(LBOX_CENTERX,LBOX_CENTERY,BOX_WIDTH);
    DO_SQUARE(RBOX_CENTERX,RBOX_CENTERY,BOX_WIDTH);

    DO_CROSS(LBOX_CENTERX,LBOX_CENTERY,3)
    DO_CROSS(RBOX_CENTERX,RBOX_CENTERY,3)
    DO_SQUARE(LBOX_CENTERX+(calibratedStick[0]*BOX_LIMIT/(2*RESX)), LBOX_CENTERY-(calibratedStick[1]*BOX_LIMIT/(2*RESX)), MARKER_WIDTH)
    DO_SQUARE(RBOX_CENTERX+(calibratedStick[3]*BOX_LIMIT/(2*RESX)), RBOX_CENTERY-(calibratedStick[2]*BOX_LIMIT/(2*RESX)), MARKER_WIDTH)

//    V_BAR(SCREEN_WIDTH/2-5,SCREEN_HEIGHT-10,((calibratedStick[4]+RESX)*BAR_HEIGHT/(RESX*2))+1l) //P1
//    V_BAR(SCREEN_WIDTH/2  ,SCREEN_HEIGHT-10,((calibratedStick[5]+RESX)*BAR_HEIGHT/(RESX*2))+1l) //P2
//    V_BAR(SCREEN_WIDTH/2+5,SCREEN_HEIGHT-10,((calibratedStick[6]+RESX)*BAR_HEIGHT/(RESX*2))+1l) //P3

    // Optimization by Mike Blandford
    {
        uint8_t x, y, len ;			// declare temporary variables
        for( x = -5, y = 4 ; y < 7 ; x += 5, y += 1 )
        {
            len = ((calibratedStick[y]+RESX)*BAR_HEIGHT/(RESX*2))+1l ;  // calculate once per loop
            V_BAR(SCREEN_WIDTH/2+x,SCREEN_HEIGHT-10, len )
        }
    }



    int8_t a = (g_eeGeneral.view == 2) ? 0 : 9+(g_eeGeneral.view-3)*6;
    int8_t b = (g_eeGeneral.view == 2) ? 6 : 12+(g_eeGeneral.view-3)*6;
    for(int8_t i=a; i<(a+3); i++) lcd_putsnAtt(2*FW-2 ,(i-a)*FH+4*FH,get_switches_string()+3*i,3,getSwitch(i+1, 0) ? INVERS : 0);
    for(int8_t i=b; i<(b+3); i++) lcd_putsnAtt(17*FW-1,(i-b)*FH+4*FH,get_switches_string()+3*i,3,getSwitch(i+1, 0) ? INVERS : 0);
  }
}

uint16_t isqrt32(uint32_t n)
{
    uint16_t c = 0x8000;
    uint16_t g = 0x8000;

    for(;;) {
        if((uint32_t)g*g > n)
            g ^= c;
        c >>= 1;
        if(c == 0)
            return g;
        g |= c;
    }
}

int16_t intpol(int16_t x, uint8_t idx) // -100, -75, -50, -25, 0 ,25 ,50, 75, 100
{
#define D9 (RESX * 2 / 8)
#define D5 (RESX * 2 / 4)
  bool    cv9 = idx >= MAX_CURVE5;
  int8_t *crv = cv9 ? g_model.curves9[idx-MAX_CURVE5] : g_model.curves5[idx];
  int16_t erg;

  x+=RESXu;
  if(x < 0) {
    erg = (int16_t)crv[0] * (RESX/4);
  } else if(x >= (RESX*2)) {
    erg = (int16_t)crv[(cv9 ? 8 : 4)] * (RESX/4);
  } else {
    int16_t a,dx;
    if(cv9){
      a   = (uint16_t)x / D9;
      dx  =((uint16_t)x % D9) * 2;
    } else {
      a   = (uint16_t)x / D5;
      dx  = (uint16_t)x % D5;
    }
    erg  = (int16_t)crv[a]*((D5-dx)/2) + (int16_t)crv[a+1]*(dx/2);
  }
  return erg / 25; // 100*D5/RESX;
}

// static variables used in perOut - moved here so they don't interfere with the stack
// It's also easier to initialize them here.
uint16_t pulses2MHz[120] = {0};
int16_t  anas [NUM_XCHNRAW] = {0};
int32_t  chans[NUM_CHNOUT] = {0};
uint32_t inacCounter = 0;
uint16_t inacSum = 0;
uint8_t  bpanaCenter = 0;
int16_t  sDelay[MAX_MIXERS] = {0};
int32_t  act   [MAX_MIXERS] = {0};
uint8_t  swOn  [MAX_MIXERS] = {0};

void perOut(int16_t *chanOut, uint8_t zeroInput)
{
  int16_t  trimA[4];
  uint8_t  anaCenter = 0;
  uint16_t d = 0;

  if(tick10ms) {
    if(s_noHi) s_noHi--;
    if(g_eeGeneral.inactivityTimer && (g_vbat100mV>49)) {
      inacCounter++;
      uint16_t tsum = 0;
      for(uint8_t i=0;i<4;i++) tsum += anas[i]/128;//div 8 -> reduce sensitivity
      if(tsum!=inacSum){
        inacSum = tsum;
        inacCounter=0;
      }
      if(inacCounter>((uint32_t)(g_eeGeneral.inactivityTimer+10)*100*60))
        if((inacCounter&0x3F)==10) beepWarn();
    }
  }

  //===========Swash Ring================
  if(g_model.swashRingValue)
    {
        uint32_t v = (int32_t(calibratedStick[ELE_STICK])*calibratedStick[ELE_STICK] +
                      int32_t(calibratedStick[AIL_STICK])*calibratedStick[AIL_STICK]);
        uint32_t q = int32_t(RESX)*g_model.swashRingValue/100;
        q *= q;
        if(v>q)
            d = isqrt32(v);
    }
  //===========Swash Ring================

  for(uint8_t i=0;i<7;i++){        // calc Sticks

    //Normalization  [0..2048] ->   [-1024..1024]

    int16_t v = anaIn(i);
    v -= g_eeGeneral.calibMid[i];
    v  =  v * (int32_t)RESX /  (max((int16_t)100,(v>0 ?
                                     g_eeGeneral.calibSpanPos[i] :
                                     g_eeGeneral.calibSpanNeg[i])));
    if(v <= -RESX) v = -RESX;
    if(v >=  RESX) v =  RESX;
    calibratedStick[i] = v; //for show in expo
    if(!(v/16)) anaCenter |= 1<<(CONVERT_MODE((i+1))-1);


    if(i<4) { //only do this for sticks
      //===========Trainer mode================
			if ( g_model.traineron )
			{
      	TrainerMix* td = &g_eeGeneral.trainer.mix[i];
      	if (td->mode && getSwitch(td->swtch, 1)) {
      	  uint8_t chStud = td->srcChn;
      	  int16_t vStud  = (g_ppmIns[chStud]- g_eeGeneral.trainer.calib[chStud]) /* *2 */ ;
					vStud /= 2 ;		// Only 2, because no *2 above
					vStud *= td->studWeight ;
					vStud /= 31 ;
					vStud *= 4 ;
      	  switch (td->mode) {
      	    case 1: v += vStud;   break; // add-mode
      	    case 2: v  = vStud;   break; // subst-mode
      	  }
				}
      }

        //===========Swash Ring================
        if(d && (i==ELE_STICK || i==AIL_STICK))
                v = int32_t(v)*g_model.swashRingValue*RESX/(int32_t(d)*100);
        //===========Swash Ring================

      uint8_t expoDrOn = GET_DR_STATE(i);
      uint8_t stkDir = v>0 ? DR_RIGHT : DR_LEFT;

      if(IS_THROTTLE(i) && g_model.thrExpo){
        v  = 2*expo((v+RESX)/2,g_model.expoData[i].expo[expoDrOn][DR_EXPO][DR_RIGHT]);
        stkDir = DR_RIGHT;
      }
      else
        v  = expo(v,g_model.expoData[i].expo[expoDrOn][DR_EXPO][stkDir]);

      int32_t x = (int32_t)v * (g_model.expoData[i].expo[expoDrOn][DR_WEIGHT][stkDir]+100)/100;
      v = (int16_t)x;
      if (IS_THROTTLE(i) && g_model.thrExpo) v -= RESX;

      //do trim -> throttle trim if applicable
      int32_t vv = 2*RESX;
      if(IS_THROTTLE(i) && g_model.thrTrim) vv = ((int32_t)g_model.trim[i]+125)*(RESX-v)/(2*RESX);

      //trim
      trimA[i] = (vv==2*RESX) ? g_model.trim[i]*2 : (int16_t)vv*2; //    if throttle trim -> trim low end
    }
    anas[i] = v; //set values for mixer
  }

  //===========BEEP CENTER================
  anaCenter &= g_model.beepANACenter;
  if(((bpanaCenter ^ anaCenter) & anaCenter)) beepWarn1();
  bpanaCenter = anaCenter;

  anas[MIX_MAX-1]  = RESX;     // MAX
  anas[MIX_FULL-1] = RESX;     // FULL
  for(uint8_t i=0;i<4;i++)    anas[i+PPM_BASE]         = (g_ppmIns[i] - g_eeGeneral.trainer.calib[i])*2; //add ppm channels
  for(uint8_t i=4;i<NUM_PPM;i++)    anas[i+PPM_BASE]   = g_ppmIns[i]*2; //add ppm channels
  for(uint8_t i=0;i<NUM_CHNOUT;i++) anas[i+CHOUT_BASE] = chans[i]; //other mixes previous outputs
  
  //===========Swash Ring================
  if(g_model.swashRingValue)
  {
      uint32_t v = ((int32_t)anas[ELE_STICK]*anas[ELE_STICK] + (int32_t)anas[AIL_STICK]*anas[AIL_STICK]);
      uint32_t q = (int32_t)RESX*g_model.swashRingValue/100;
      q *= q;
      if(v>q)
      {
          uint16_t d = isqrt32(v);
          anas[ELE_STICK] = (int32_t)anas[ELE_STICK]*g_model.swashRingValue*RESX/((int32_t)d*100);
          anas[AIL_STICK] = (int32_t)anas[AIL_STICK]*g_model.swashRingValue*RESX/((int32_t)d*100);
      }
  }

#define REZ_SWASH_X(x)  ((x) - (x)/8 - (x)/128 - (x)/512)   //  1024*sin(60) ~= 886
#define REZ_SWASH_Y(x)  ((x))   //  1024 => 1024

  if(g_model.swashType)
  {
      int16_t vp = anas[ELE_STICK]+trimA[ELE_STICK];
      int16_t vr = anas[AIL_STICK]+trimA[AIL_STICK];
      int16_t vc = 0;
      if(g_model.swashCollectiveSource)
          vc = anas[g_model.swashCollectiveSource-1];

      if(g_model.swashInvertELE) vp = -vp;
      if(g_model.swashInvertAIL) vr = -vr;
      if(g_model.swashInvertCOL) vc = -vc;

      switch (g_model.swashType)
      {
      case (SWASH_TYPE_120):
          vp = REZ_SWASH_Y(vp);
          vr = REZ_SWASH_X(vr);
          anas[MIX_CYC1-1] = vc - vp;
          anas[MIX_CYC2-1] = vc + vp/2 + vr;
          anas[MIX_CYC3-1] = vc + vp/2 - vr;
          break;
      case (SWASH_TYPE_120X):
          vp = REZ_SWASH_X(vp);
          vr = REZ_SWASH_Y(vr);
          anas[MIX_CYC1-1] = vc - vr;
          anas[MIX_CYC2-1] = vc + vr/2 + vp;
          anas[MIX_CYC3-1] = vc + vr/2 - vp;
          break;
      case (SWASH_TYPE_140):
          vp = REZ_SWASH_Y(vp);
          vr = REZ_SWASH_Y(vr);
          anas[MIX_CYC1-1] = vc - vp;
          anas[MIX_CYC2-1] = vc + vp + vr;
          anas[MIX_CYC3-1] = vc + vp - vr;
          break;
      case (SWASH_TYPE_90):
          vp = REZ_SWASH_Y(vp);
          vr = REZ_SWASH_Y(vr);
          anas[MIX_CYC1-1] = vc - vp;
          anas[MIX_CYC2-1] = vc + vr;
          anas[MIX_CYC3-1] = vc - vr;
          break;
      default:
          break;
      }
  }

  if(tick10ms) trace(); //trace thr 0..32  (/32)

  memset(chans,0,sizeof(chans));        // All outputs to 0

  if(zeroInput) //zero input for setStickCenter()
    for(uint8_t i=0;i<4;i++)
      if(!IS_THROTTLE(i)) {
        anas[i]  = 0;
        trimA[i] = 0;
      }

   uint8_t mixWarning = 0;
    //========== MIXER LOOP ===============
    for(uint8_t i=0;i<MAX_MIXERS;i++){
      MixData *md = mixaddress( i ) ;

      if((md->destCh==0) || (md->destCh>NUM_CHNOUT)) break;

      //Notice 0 = NC switch means not used -> always on line
      int16_t v  = 0;
      uint8_t swTog;

      //swOn[i]=false;
      if(!getSwitch(md->swtch,1)){ // switch on?  if no switch selected => on
        swTog = swOn[i];
        swOn[i] = false;
        if(md->srcRaw!=MIX_MAX && md->srcRaw!=MIX_FULL) continue;// if not MAX or FULL - next loop
        if(md->mltpx==MLTPX_REP) continue; // if switch is off and REPLACE then off
        v = (md->srcRaw == MIX_FULL ? -RESX : 0); // switch is off and it is either MAX=0 or FULL=-512
      }
      else {
        swTog = !swOn[i];
        swOn[i] = true;
        uint8_t k = md->srcRaw-1;
        v = anas[k]; //Switch is on. MAX=FULL=512 or value.
        if(k>=CHOUT_BASE && (k<i)) v = chans[k]; // if we've already calculated the value - take it instead // anas[i+CHOUT_BASE] = chans[i]
        if(md->mixWarn) mixWarning |= 1<<(md->mixWarn-1); // Mix warning
      }

      //========== INPUT OFFSET ===============
      if(md->sOffset) v += calc100toRESX(md->sOffset);

      //========== DELAY and PAUSE ===============
      if (md->speedUp || md->speedDown || md->delayUp || md->delayDown)  // there are delay values
      {
#define DEL_MULT 256

        //if(init) {
          //act[i]=(int32_t)v*DEL_MULT;
          //swTog = false;
        //}
        int16_t diff = v-act[i]/DEL_MULT;

        if(swTog) {
            //need to know which "v" will give "anas".
            //curves(v)*weight/100 -> anas
            // v * weight / 100 = anas => anas*100/weight = v
          if(md->mltpx==MLTPX_REP)
          {
              act[i] = (int32_t)anas[md->destCh-1+CHOUT_BASE]*DEL_MULT;
              act[i] *=100;
              if(md->weight) act[i] /= md->weight;
          }
          diff = v-act[i]/DEL_MULT;
          if(diff) sDelay[i] = (diff<0 ? md->delayUp :  md->delayDown) * 100;
        }

        if(sDelay[i]){ // perform delay
            if(tick10ms) sDelay[i]--;
            v = act[i]/DEL_MULT;
            diff = 0;
        }

        if(diff && (md->speedUp || md->speedDown)){
          //rate = steps/sec => 32*1024/100*md->speedUp/Down
          //act[i] += diff>0 ? (32768)/((int16_t)100*md->speedUp) : -(32768)/((int16_t)100*md->speedDown);
          //-100..100 => 32768 ->  100*83886/256 = 32768,   For MAX we divide by 2 sincde it's asymmetrical
          if(tick10ms) {
              int32_t rate = (int32_t)DEL_MULT*2048*100;
              if(md->weight) rate /= abs(md->weight);
              act[i] = (diff>0) ? ((md->speedUp>0)   ? act[i]+(rate)/((int16_t)100*md->speedUp)   :  (int32_t)v*DEL_MULT) :
                                  ((md->speedDown>0) ? act[i]-(rate)/((int16_t)100*md->speedDown) :  (int32_t)v*DEL_MULT) ;
          }

          if(((diff>0) && (v<(act[i]/DEL_MULT))) || ((diff<0) && (v>(act[i]/DEL_MULT)))) act[i]=(int32_t)v*DEL_MULT; //deal with overflow
          v = act[i]/DEL_MULT;
        }
      }


      //========== CURVES ===============
      switch(md->curve){
        case 0:
          break;
        case 1:
          if(md->srcRaw == MIX_FULL) //FUL
          {
            if( v<0 ) v=-RESX;   //x|x>0
            else      v=-RESX+2*v;
          }else{
            if( v<0 ) v=0;   //x|x>0
          }
          break;
        case 2:
          if(md->srcRaw == MIX_FULL) //FUL
          {
            if( v>0 ) v=RESX;   //x|x<0
            else      v=RESX+2*v;
          }else{
            if( v>0 ) v=0;   //x|x<0
          }
          break;
        case 3:       // x|abs(x)
          v = abs(v);
          break;
        case 4:       //f|f>0
          v = v>0 ? RESX : 0;
          break;
        case 5:       //f|f<0
          v = v<0 ? -RESX : 0;
          break;
        case 6:       //f|abs(f)
          v = v>0 ? RESX : -RESX;
          break;
        default: //c1..c16
          v = intpol(v, md->curve - 7);
      }

      //========== TRIM ===============
      if((md->carryTrim==0) && (md->srcRaw>0) && (md->srcRaw<=4)) v += trimA[md->srcRaw-1];  //  0 = Trim ON  =  Default

      //========== MULTIPLEX ===============
      int32_t dv = (int32_t)v*md->weight;
      switch(md->mltpx){
        case MLTPX_REP:
          chans[md->destCh-1] = dv;
          break;
        case MLTPX_MUL:
          chans[md->destCh-1] *= dv/100l;
          chans[md->destCh-1] /= RESXl;
          break;
        default:  // MLTPX_ADD
          chans[md->destCh-1] += dv; //Mixer output add up to the line (dv + (dv>0 ? 100/2 : -100/2))/(100);
          break;
        }
    }

  //========== MIXER WARNING ===============
  //1= 00,08
  //2= 24,32,40
  //3= 56,64,72,80
		{
			uint16_t tmr10ms ;
			tmr10ms = get_tmr10ms() ;
      if(mixWarning & 1) if(((tmr10ms&0xFF)==  0)) beepWarn1();
      if(mixWarning & 2) if(((tmr10ms&0xFF)== 64) || ((tmr10ms&0xFF)== 72)) beepWarn1();
      if(mixWarning & 4) if(((tmr10ms&0xFF)==128) || ((tmr10ms&0xFF)==136) || ((tmr10ms&0xFF)==144)) beepWarn1();
		}

  //========== LIMITS ===============
  for(uint8_t i=0;i<NUM_CHNOUT;i++){
      // chans[i] holds data from mixer.   chans[i] = v*weight => 1024*100
      // later we multiply by the limit (up to 100) and then we need to normalize
      // at the end chans[i] = chans[i]/100 =>  -1024..1024
      // interpolate value with min/max so we get smooth motion from center to stop
      // this limits based on v original values and min=-1024, max=1024  RESX=1024

      int32_t q = chans[i];// + (int32_t)g_model.limitData[i].offset*100; // offset before limit

      chans[i] /= 100; // chans back to -1024..1024
      ex_chans[i] = chans[i]; //for getswitch

      int16_t ofs = g_model.limitData[i].offset;
      int16_t lim_p = 10*(g_model.limitData[i].max+100);
      int16_t lim_n = 10*(g_model.limitData[i].min-100); //multiply by 10 to get same range as ofs (-1000..1000)
      if(ofs>lim_p) ofs = lim_p;
      if(ofs<lim_n) ofs = lim_n;

      if(q) q = (q>0) ?
                q*((int32_t)lim_p-ofs)/100000 :
               -q*((int32_t)lim_n-ofs)/100000 ; //div by 100000 -> output = -1024..1024

      q += calc1000toRESX(ofs);
      lim_p = calc1000toRESX(lim_p);
      lim_n = calc1000toRESX(lim_n);
      if(q>lim_p) q = lim_p;
      if(q<lim_n) q = lim_n;
      if(g_model.limitData[i].revert) q=-q;// finally do the reverse.

      if(g_model.safetySw[i].swtch)  //if safety sw available for channel check and replace val if needed
          if(getSwitch(g_model.safetySw[i].swtch,0)) q = calc100toRESX(g_model.safetySw[i].val);

      cli();
      chanOut[i] = q; //copy consistent word to int-level
      sei();
  }
}



/******************************************************************************
  the functions below are from int-level
  the functions below are from int-level
  the functions below are from int-level
******************************************************************************/

void setupPulses()
{
  switch(g_model.protocol)
  {
    case PROTO_PPM:
      setupPulsesPPM();
      break;
    case PROTO_SILV_A:
    case PROTO_SILV_B:
    case PROTO_SILV_C:
      setupPulsesSilver();
      break;
    case PROTO_TRACER_CTP1009:
      setupPulsesTracerCtp1009();
      break;
    case PROTO_PPRZ_XBEE_API:
    case PROTO_PPRZ_TRANSPARENT:
    case PROTO_PPRZ_LAIRD:
#ifdef PAPARAZZI
      PPRZ_setupPulses(); 
#endif      
      break;
  }
}

//inline int16_t reduceRange(int16_t x)  // for in case we want to have room for subtrims
//{
//    return x-(x/4);  //512+128 =? 640,  640 - 640/4  == 640 * 3/4 => 480 (just below 500msec - it can still reach 500 with offset)
//}

void setupPulsesPPM() // changed 10/05/2010 by dino Issue 128
{
#define PPM_CENTER 1200*2
    int16_t PPM_range = g_model.extendedLimits ? 640*2 : 512*2;   //range of 0.7..1.7msec

    //Total frame length = 22.5msec
    //each pulse is 0.7..1.7ms long with a 0.3ms stop tail
    //The pulse ISR is 2mhz that's why everything is multiplied by 2
    uint8_t j=0;
    uint8_t p=8+g_model.ppmNCH*2; //Channels *2
    uint16_t q=(g_model.ppmDelay*50+300)*2; //Stoplen *2
    uint16_t rest=22500u*2-q; //Minimum Framelen=22.5 ms
    if(p>9) rest=p*(1720u*2 + q) + 4000u*2; //for more than 9 channels, frame must be longer
    for(uint8_t i=0;i<p;i++){ //NUM_CHNOUT
        int16_t v = max(min(g_chans512[i],PPM_range),-PPM_range) + PPM_CENTER;
        rest-=(v+q);
        pulses2MHz[j++]=q;
        pulses2MHz[j++]=v;  //pulses2MHz[j++] = v - q +600
    }
    pulses2MHz[j++]=q;
    pulses2MHz[j++]=rest;
    pulses2MHz[j++]=0;
}


uint16_t *pulses2MHzPtr;
#define BITLEN (600u*2)
void _send_hilo(uint16_t hi,uint16_t lo)
{
  *pulses2MHzPtr++=hi; *pulses2MHzPtr++=lo;
}
#define send_hilo_silv( hi, lo) _send_hilo( (hi)*BITLEN,(lo)*BITLEN )

void sendBitSilv(uint8_t val)
{
  send_hilo_silv((val)?2:1,(val)?2:1);
}
void send2BitsSilv(uint8_t val)
{
  sendBitSilv(val&2);sendBitSilv(val&1);
}
// _ oder - je 0.6ms  (gemessen 0.7ms)
//
//____-----_-_-_--_--_   -_--__  -_-_-_-_  -_-_-_-_  --__--__-_______
//         trailer        chan     m1         m2
//
//see /home/thus/txt/silverlit/thus.txt
//m1, m2 most significant bit first |m1-m2| <= 9
//chan: 01=C 10=B
//chk = 0 - chan -m1>>2 -m1 -m2>>2 -m2
//<= 500us Probleme
//>= 650us Probleme
//periode orig: 450ms
void setupPulsesSilver()
{
  int8_t chan=1; //chan 1=C 2=B 0=A?

  switch(g_model.protocol)
  {
    case PROTO_SILV_A: chan=0; break;
    case PROTO_SILV_B: chan=2; break;
    case PROTO_SILV_C: chan=1; break;
  }

  int8_t m1 = (uint16_t)(g_chans512[0]+1024)*2 / 256;
  int8_t m2 = (uint16_t)(g_chans512[1]+1024)*2 / 256;
  if (m1 < 0)    m1=0;
  if (m2 < 0)    m2=0;
  if (m1 > 15)   m1=15;
  if (m2 > 15)   m2=15;
  if (m2 > m1+9) m1=m2-9;
  if (m1 > m2+9) m2=m1-9;
  //uint8_t i=0;
  pulses2MHzPtr=pulses2MHz;
  send_hilo_silv(5,1); //idx 0 erzeugt pegel=0 am Ausgang, wird  als high gesendet
  send2BitsSilv(0);
  send_hilo_silv(2,1);
  send_hilo_silv(2,1);

  send2BitsSilv(chan); //chan 1=C 2=B 0=A?
  uint8_t sum = 0 - chan;

  send2BitsSilv(m1>>2); //m1
  sum-=m1>>2;
  send2BitsSilv(m1);
  sum-=m1;

  send2BitsSilv(m2>>2); //m2
  sum-=m2>>2;
  send2BitsSilv(m2);
  sum-=m2;

  send2BitsSilv(sum); //chk

  sendBitSilv(0);
  pulses2MHzPtr--;
  send_hilo_silv(50,0); //low-impuls (pegel=1) ueberschreiben


}



/*
  TRACE CTP-1009
   - = send 45MHz
   _ = send nix
    start1       0      1           start2
  -------__     --_    -__         -----__
   7ms   2     .8 .4  .4 .8         5   2

 frame:
  start1  24Bits_1  start2  24_Bits2

 24Bits_1:
  7 x Bits  Throttle lsb first
  1 x 0

  6 x Bits  rotate lsb first
  1 x Bit   1=rechts
  1 x 0

  4 x Bits  chk5 = nib2 ^ nib4
  4 x Bits  chk6 = nib1 ^ nib3

 24Bits_2:
  7 x Bits  Vorwaets lsb first 0x3f = mid
  1 x 1

  7 x Bits  0x0e lsb first
  1 x 1

  4 x Bits  chk5 = nib2 ^ nib4
  4 x Bits  chk6 = nib1 ^ nib3

 */

#define BIT_TRA (400u*2)
void sendBitTra(uint8_t val)
{
  if(val) _send_hilo( BIT_TRA*1 , BIT_TRA*2 );
  else    _send_hilo( BIT_TRA*2 , BIT_TRA*1 );
}
void sendByteTra(uint8_t val)
{
  for(uint8_t i=0; i<8; i++, val>>=1) sendBitTra(val&1);
}
void setupPulsesTracerCtp1009()
{
  pulses2MHzPtr=pulses2MHz;
  static bool phase;
  if( (phase=!phase) ){
    uint8_t thr = min(127u,(uint16_t)(g_chans512[0]+1024+8) /  16u);
    uint8_t rot;
    if (g_chans512[1] >= 0)
    {
      rot = min(63u,(uint16_t)( g_chans512[1]+16) / 32u) | 0x40;
    }else{
      rot = min(63u,(uint16_t)(-g_chans512[1]+16) / 32u);
    }
    sendByteTra(thr);
    sendByteTra(rot);
    uint8_t chk=thr^rot;
    sendByteTra( (chk>>4) | (chk<<4) );
    _send_hilo( 5000*2, 2000*2 );
  }else{
    uint8_t fwd = min(127u,(uint16_t)(g_chans512[2]+1024) /  16u) | 0x80;
    sendByteTra(fwd);
    sendByteTra(0x8e);
    uint8_t chk=fwd^0x8e;
    sendByteTra( (chk>>4) | (chk<<4) );
    _send_hilo( 7000*2, 2000*2 );
  }
  *pulses2MHzPtr++=0;
  if((pulses2MHzPtr-pulses2MHz) >= (signed)DIM(pulses2MHz)) alert(PSTR("pulse tab overflow"));
}
