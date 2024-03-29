/*
 * Centralina tettoia - versione 2
 * 
 * modifiche 12/1/2018
 * modo fuori casa non più con chiave ma con uscita temporizzata
 * modo allarme: off, in casa click, fuori casa click lungo. Dai modi allarmi con un click li spegne
 * buzzer per il tempo d'uscita
 * Gestisce l'apertura della porta dopo il cancello
 * 
 * accende e spegne le 3 luci alla pressione del pulsante
 * parte da tutto spento (acceso =low)
 * al click accende/spegne la lanterna
 * al click lungo accende/spegne tutto
 */
#include <Arduino.h>
#include <EEPROM.h>
#include "Antirimbalzo.h"
#include "Proto485.h"
#include "ControlloUscita.h"
//#define DEBUG
#ifdef DEBUG
 #define DEBUG_PRINT(x, ...)  Serial.print (x, ##__VA_ARGS__)
 #define DEBUG_PRINTLN(x, ...)  Serial.println (x, ##__VA_ARGS__)
#else
 #define DEBUG_PRINT(x, ...)
 #define DEBUG_PRINTLN(x, ...)  
#endif

// 6 relè 
#define RELEFARI A0
#define RELESIRENA 3
#define RELELAMPADA 4
#define RELELANTERNA 5
#define RELEAPRIPORTA 6
#define RELEAPRICANCELLO 7
// 7 ingressi
#define PULSANTELUCI 8
#define PULSANTEMODOALLARME 9
#define MAGNETICI 10
#define BUZZER A1
#define MOVIMENTO A2
#define CREPUSCOLARE A3
// 1 uscita
#define LEDSTATO A4
#define LEDROSSO A5
#define TXENABLE 12
#define INTERVALLOCONTROLLOCREPUSCOLARE 3000

// tempi gestione centralina
unsigned int tempoAllarme; // in secondi
byte tempoUscita; // in minuti
byte tempoEntrata; // in secondi
byte tempoFari; // tempo accensione fari da pir in minuti

// contatori timeout
unsigned long int tInizioTimeoutUscita, tInizioTimeoutEntrata, tInizioTimeoutAllarme;

unsigned int soglia_crepuscolare;
byte isteresi_crepuscolare;
bool notte, faridapir,precstatoapricencello, apriancheporta;
typedef enum {DISARMATO, INCASA, ALLARME, FINEALLARME, TIMEOUTUSCITA, FUORICASA, TIMEOUTENTRATA} modalitaantifurto;
modalitaantifurto modoantifurto; // 0=spento, 1=armato non in casa, 2=armato in casa

Antirimbalzo swLuci;
Antirimbalzo swAntifurto;
Antirimbalzo pir;
ControlloUscita led(LEDSTATO,false,false);
ControlloUscita ledrosso(LEDROSSO,false,false);
ControlloUscita buzzer(BUZZER,false,false);
// relè
ControlloUscita apricancello(RELEAPRICANCELLO,false,false);
ControlloUscita apriporta(RELEAPRIPORTA,false,false);
ControlloUscita sirena(RELESIRENA,true,false);
ControlloUscita fari(RELEFARI,true,false);
ControlloUscita lampada(RELELAMPADA,true,false);
ControlloUscita lanterna(RELELANTERNA,true,false);
// comm
Proto485 comm(&Serial,TXENABLE,true);


#define LEDPIN LED_BUILTIN
void setup() {
  Serial.begin(9600);

  //pinMode(LEDPIN, OUTPUT);
  pinMode(PULSANTELUCI, INPUT_PULLUP);
  pinMode(PULSANTEMODOALLARME, INPUT_PULLUP);
  pinMode(MAGNETICI, INPUT_PULLUP);
  pinMode(MOVIMENTO, INPUT_PULLUP);
  pinMode(CREPUSCOLARE, INPUT);

  digitalWrite(TXENABLE, LOW);
  pinMode(TXENABLE, OUTPUT);
  setDisarmato();
  
  // leggi parametri da eeprom
  tempoAllarme=EEPROM.read(0);
  isteresi_crepuscolare=EEPROM.read(1);
  EEPROM.get(2,soglia_crepuscolare);
  tempoFari=EEPROM.read(4);
  tempoUscita=EEPROM.read(5);
  tempoEntrata=EEPROM.read(6);

  //
  notte=false;
  modoantifurto=DISARMATO;

  // output
  DEBUG_PRINT("tempoAllarme=");
  DEBUG_PRINT(tempoAllarme);
  DEBUG_PRINT("s soglia=");
  DEBUG_PRINT(soglia_crepuscolare);
  DEBUG_PRINT(" tempo fari=");
  DEBUG_PRINT(tempoFari);
  DEBUG_PRINT("m ister=");
  DEBUG_PRINTLN(isteresi_crepuscolare);
  DEBUG_PRINT(" tempoUscita=");
  DEBUG_PRINTLN(tempoUscita);
  DEBUG_PRINT("m tempoEntrata=");
  DEBUG_PRINTLN(tempoEntrata);
  DEBUG_PRINT("s F_CPU=");
  DEBUG_PRINTLN(F_CPU,DEC);

  // setup ingressi
  swLuci.tDurataClickLungo=350;
  swLuci.cbClickCorto=PulsanteLuciClick;
  swLuci.cbClickLungo=PulsanteLuciLongClick;
  swAntifurto.cbClickCorto=PulsanteAntifurtoClick;
  swAntifurto.cbClickLungo=PulsanteAntifurtoClickLungo;
  swAntifurto.tDurataClickLungo=350;
  pir.cbInizioStatoOn=PirAttivato;
  pir.tPeriodoBlackOut=2000;
  comm.cbElaboraComando=ElaboraComando;
  TrasmettiStatoSCheda();
}

void loop() {
  // elabora ingressi
  swLuci.Elabora(digitalRead(PULSANTELUCI)==LOW);
  swAntifurto.Elabora(digitalRead(PULSANTEMODOALLARME)==LOW);
  pir.Elabora(digitalRead(MOVIMENTO)==HIGH);
  // elabora uscite
  led.Elabora();
  ledrosso.Elabora();
  sirena.Elabora();
  apricancello.Elabora();
  apriporta.Elabora();
  fari.Elabora();
  lanterna.Elabora();
  lampada.Elabora();
  buzzer.Elabora();
  
  // elabora funzioni
  if(digitalRead(MAGNETICI)==HIGH)
    ElaboraAperturaMagnetici();

  ElaboraTimeoutAntifurto();
  ElaboraCrepuscolare();
  if(Serial.available()) 
  {
    comm.ProcessaDatiSeriali(Serial.read());
  }
  if(apricancello.Completato() && precstatoapricencello)
  {
    precstatoapricencello=false;
    if(apriancheporta) {apriporta.OnOff(700,1000);
      apriancheporta = false;}
    }
}

// ingressi attivati
void PulsanteLuciClick() {
  if(lampada.isOn())
    SpegniLampada();
  else
    AccendiLampada();
}

void AccendiLampada() {
  lampada.On();
  comm.Tx('g',0,0);
}
void SpegniLampada() {
  lampada.Off();
  comm.Tx('h', 0, 0);
}
void AccendiFari() {
  fari.On();
  comm.Tx('e',0,0);
}
void SpegniFari() {
  fari.Off();
  comm.Tx('f', 0, 0);
}

void PulsanteLuciLongClick() {
  if(fari.isOn()) {
    SpegniFari();
    SpegniLampada();
  } else {
    AccendiLampada();
    AccendiFari();
  }
}

void PulsanteAntifurtoClick() {
  switch(modoantifurto) {
    case DISARMATO:
      setInCasa();
      break;
    case INCASA:
      setDisarmato();
      break;
    case FUORICASA:
      setDisarmato();
      break;
    case FINEALLARME:
      setDisarmato();
      break;
    case ALLARME:
      setDisarmato();
      break;
    case TIMEOUTUSCITA:
      setDisarmato();
      break;
    case TIMEOUTENTRATA:
      setDisarmato();
      break;
    }
}

void PulsanteAntifurtoClickLungo() {
  switch(modoantifurto) {
    case DISARMATO:
      setInizioTimeoutUscita();
      break;
    case TIMEOUTUSCITA:
      setDisarmato();
      break;
    case FUORICASA:
      setDisarmato();
      break;
    case TIMEOUTENTRATA:
      setDisarmato();
      break;
    }
}

void ElaboraCrepuscolare() {
  static unsigned long int tultimocontrollo;
  if((millis() - tultimocontrollo) < INTERVALLOCONTROLLOCREPUSCOLARE) return;
  tultimocontrollo=millis();
  unsigned int val = 1024-analogRead(CREPUSCOLARE);
  if(!notte && (val<soglia_crepuscolare-isteresi_crepuscolare)) {notte=true; comm.Tx('D',0,0); lanterna.On(); return;}
  if(notte && (val>soglia_crepuscolare+isteresi_crepuscolare)) {notte=false; comm.Tx('E',0,0); lanterna.Off();
    SpegniLampada();
    SpegniFari();
    return;
  }
}

void PirAttivato() {
  AccendiFariSeNotte();
  comm.Tx('Z',0,0);
}

void AccendiFariSeNotte() {
  if(notte) {
    if(!fari.isOn()) fari.On(tempoFari*60000);
  }
}

void impostaled(int Ton, int Toff) {
  led.OndaQuadra(Ton,Toff);
}

void ElaboraAperturaMagnetici() 
{
  switch(modoantifurto) {
    case INCASA:
      setAllarme();
      break;
    case FUORICASA:
      setInizioTimeoutEntrata();
      break;
    }
}

void ElaboraTimeoutAntifurto() {
  switch(modoantifurto) 
  {
    case TIMEOUTUSCITA:
      if((millis()- tInizioTimeoutUscita)>tempoUscita*60000)
        setFuoriCasa();
      break;
    case TIMEOUTENTRATA:
      if((millis()- tInizioTimeoutEntrata)>tempoEntrata*1000)
        setAllarme();
      break;
    case ALLARME:
      if((millis()- tInizioTimeoutAllarme)>tempoAllarme*1000)
        setFineAllarme();
      break;
  }
}

void StampaModoAllarme()
{
  DEBUG_PRINT(" modoalm=");
  DEBUG_PRINTLN(modoantifurto);
}

void ApricancelloEPorta(bool ancheporta)
{
  apricancello.OnOff(700,1000);
  precstatoapricencello=true;
  apriancheporta = ancheporta;
}

void ElaboraComando(byte comando,byte *bytesricevuti,byte len) {
  switch(comando) {
    case 'C': // pulsante apricancello
      ApricancelloEPorta(false);
      AccendiFariSeNotte();
      break;
    case 'T': // tag ricevuto
      ApricancelloEPorta(true);
      AccendiFariSeNotte();
      setDisarmato();
      break;
    case 'N':
      setInCasa();
      break;
    case 'O':
      setDisarmato();
      break;
    case 'Q':
      MemorizzaParametro(bytesricevuti,len);
      break;
    case 'P':
      setInizioTimeoutUscita();
      break;
    case 'L':
      //richiesta stato
      TrasmettiStatoSCheda();
      break;
    // movimento da cancello
    case 'B':
      AccendiFariSeNotte();
      break;
    case 'c':
      AccendiLampada();
      break;
    case 'd':
      SpegniLampada();
      break;
    case 'a':
      AccendiFari();
      break;
    case 'b':
      SpegniFari();
      break;
  }
}

void MemorizzaParametro(byte *bytesricevuti,byte len) {
	switch(bytesricevuti[0]) {
		case 'S':
      soglia_crepuscolare=bytesricevuti[1]+bytesricevuti[2]*256;
      if(soglia_crepuscolare>1000) return;
			EEPROM.put(2,soglia_crepuscolare);
			break;
		case 'I':
      isteresi_crepuscolare=bytesricevuti[1];
			EEPROM.put(1,isteresi_crepuscolare);
			break;
		case 'D':
			tempoAllarme=bytesricevuti[1];
			EEPROM.put(0,bytesricevuti[1]);
			break;
    case 'T':
      tempoFari=bytesricevuti[1];
			EEPROM.put(4,tempoFari);
			break;
    case 'Y':
      tempoUscita=bytesricevuti[1];
			EEPROM.put(5,tempoUscita);
			break;
    case 'U':
      tempoEntrata=bytesricevuti[1];
			EEPROM.put(6,tempoEntrata);
			break;
	}
}

// typedef enum {DISARMATO, INCASA, ALLARME, FINEALLARME, TIMEOUTUSCITA, FUORICASA, TIMEOUTENTRATA} modalitaantifurto;

void setDisarmato() {
  modoantifurto=DISARMATO;
  comm.Tx('S',0,0);
  impostaled(30,1500);
  ledrosso.Off();
  buzzer.Off();
  sirena.Off();
  StampaModoAllarme();
}

// armato in casa se le porte sono aperte non entra in questo modo
void setInCasa() {
  if(digitalRead(MAGNETICI)==HIGH) return;
  comm.Tx('U',0,0);
  modoantifurto=INCASA;
  impostaled(50,50);
  StampaModoAllarme();
}

// armato in casa se le porte sono aperte non entra in questo modo
void setAllarme() {
  comm.Tx('J',0,0);
  modoantifurto=ALLARME;
  ledrosso.On();
  sirena.On();
  buzzer.Off();
  tInizioTimeoutAllarme = millis();
  StampaModoAllarme();
}

void setFineAllarme() {
  sirena.Off();
  comm.Tx('K',0,0);
  modoantifurto=FINEALLARME;
  StampaModoAllarme();
}

void setInizioTimeoutUscita() {
  modoantifurto=TIMEOUTUSCITA;
  tInizioTimeoutUscita=millis();
  buzzer.OndaQuadra(500,500);
  impostaled(300,300);
  StampaModoAllarme();
}

// armato ma non in casa se le porte sono aperte e la chiave è on non entra in questo modo
void setFuoriCasa() {
  comm.Tx('R',0,0);
  modoantifurto=FUORICASA;
  buzzer.Off();
  impostaled(50,50);
  StampaModoAllarme();
}

void setInizioTimeoutEntrata() {
  modoantifurto=TIMEOUTENTRATA;
  tInizioTimeoutEntrata=millis();
  buzzer.OndaQuadra(500,500);
  impostaled(300,300);
  StampaModoAllarme();
}


/*
 B0
 bits:
 10
 00 disarmato
 01 armato
 10 in casa
 11 niente

*/
void TrasmettiStatoSCheda() {
  unsigned int valore_crepuscolare = 1024-analogRead(CREPUSCOLARE);
  byte b0 = modoantifurto, b1 = 0;
  if(digitalRead(MAGNETICI)==LOW) b0=b0 | 0x08;
  if(notte) b0=b0 | 0x10;
  if(digitalRead(MOVIMENTO)==HIGH) b0=b0 | 0x20;
  if(digitalRead(RELEFARI)==LOW) b1=b1 | 0x01;
  if(digitalRead(RELEAPRICANCELLO)==LOW) b1=b1 | 0x02;
  if(digitalRead(RELELAMPADA)==LOW) b1=b1 | 0x04;
  if(digitalRead(RELELANTERNA)==LOW) b1=b1 | 0x08;
  if(digitalRead(RELESIRENA)==LOW) b1=b1 | 0x10;
  if(digitalRead(RELEAPRIPORTA)==LOW) b1=b1 | 0x20;
  byte par[11];
  par[0]=b0;
  par[1]=b1;
  par[2]=tempoAllarme;
  par[3]=isteresi_crepuscolare;
  par[4]=(soglia_crepuscolare >> 8);
  par[5]=(soglia_crepuscolare & 0xff);
  par[6]=tempoFari;
  par[7]=tempoUscita;
  par[8]=tempoEntrata;
  par[9]=(valore_crepuscolare >> 8);
  par[10]=(valore_crepuscolare & 0xff);
  comm.Tx('M',11,(const char *)par);
}
