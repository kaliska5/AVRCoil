void Kalibracja();
void Czytaj_Po_Kalibracji();
void Czytaj_TS_NK();

extern volatile uint16_t F,ADC_Tmp,Xs,Ys,Xzakres,Yzakres;
extern int sx;
extern int dx;
extern int jx;
extern uint16_t Xzero;
extern uint16_t Yzero;
extern uint16_t Xmax;
extern uint16_t Ymax;
extern volatile uint32_t Xx;
extern volatile uint32_t Yy;
extern volatile uint16_t X,Y;
extern EEMEM uint16_t EEP_Xzero,EEP_Yzero,EEP_Xzakres,EEP_Yzakres;
extern EEMEM uint8_t EEP_Kalibracja;