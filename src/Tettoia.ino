/*
 * Centralina tettoia - versione 1
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
#define DEBUG
#ifdef DEBUG
 #define DEBUG_PRINT(x, ...)  Serial.print (x, ##__VA_ARGS__)
 #define DEBUG_PRINTLN(x, ...)  Serial.println (x, ##__VA_ARGS__)
#else
 #define DEBUG_PRINT(x, ...)
 #define DEBUG_PRINTLN(x, ...)  
#endif

// 6 relè 
#define RELEFARI A0
#define RELEBOH 3
#define RELELAMPADA 4
#define RELELANTERNA 5
#define RELESIRENA 6
#define RELEAPRICANCELLO 7
// 7 ingressi
#define PULSANTELUCI 8
#define PULSANTEMODOALLARME 9
#define MAGNETICI 10
#define CHIAVE A1
#define MOVIMENTO A2
#define CREPUSCOLARE A3
// 1 uscita
#define LEDSTATO A4
#define TXENABLE 12
#define INTERVALLOCONTROLLOCREPUSCOLARE 3000

//unsigned long tinizioallarme,tinizioapricancello,tinizioaccensionefari;
unsigned int tdurataallarme; // in secondi

unsigned int soglia_crepuscolare;
byte isteresi_crepuscolare;
bool notte, faridapir;
typedef enum {DISARMATO, ARMATO, INCASA} modalitaantifurto;
modalitaantifurto modoantifurto; // 0=spento, 1=armato non in casa, 2=armato in casa

Antirimbalzo swLuci;
Antirimbalzo swAntifurto;
Antirimbalzo pir;
ControlloUscita led(LEDSTATO,false,false);
// relè
ControlloUscita sirena(RELESIRENA,true,false);
ControlloUscita apricancello(RELEAPRICANCELLO,true,false);
ControlloUscita fari(RELEFARI,true,false);
ControlloUscita lampada(RELELAMPADA,true,false);
ControlloUscita lanterna(RELELANTERNA,true,false);
ControlloUscita boh(RELEBOH,true,false);
// comm
Proto485 comm(TXENABLE);


#define LEDPIN LED_BUILTIN
void setup() {
  Serial.begin(9600);

  //pinMode(LEDPIN, OUTPUT);
  pinMode(PULSANTELUCI, INPUT_PULLUP);
  pinMode(PULSANTEMODOALLARME, INPUT_PULLUP);
  pinMode(MAGNETICI, INPUT_PULLUP);
  pinMode(CHIAVE, INPUT_PULLUP);
  pinMode(MOVIMENTO, INPUT_PULLUP);
  pinMode(LEDSTATO, OUTPUT);
  pinMode(CREPUSCOLARE, INPUT_PULLUP);

  digitalWrite(TXENABLE, LOW);
  pinMode(TXENABLE, OUTPUT);
  setDisarmato();
  
  // leggi parametri da eeprom
  tdurataallarme=EEPROM.read(0)*1000;
  isteresi_crepuscolare=EEPROM.read(1);
  EEPROM.get(2,soglia_crepuscolare);

  //
  notte=false;
  modoantifurto=DISARMATO;

  // output
  DEBUG_PRINT("dural=");
  DEBUG_PRINT(tdurataallarme);
  DEBUG_PRINT(" soglia=");
  DEBUG_PRINT(soglia_crepuscolare);
  DEBUG_PRINT(" ister=");
  DEBUG_PRINTLN(isteresi_crepuscolare);

  // setup ingressi
  swLuci.tDurataClickLungo=350;
  swLuci.cbClickCorto=PulsanteLuciClick;
  swLuci.cbClickLungo=PulsanteLuciLongClick;
  swAntifurto.cbClickCorto=PulsanteAntifurtoClick;
  pir.cbClickCorto=PirAttivato;
  comm.cbElaboraComando=ElaboraComando;
}

void loop() {
  // elabora ingressi
  swLuci.Elabora(digitalRead(PULSANTELUCI)==LOW);
  swAntifurto.Elabora(digitalRead(PULSANTEMODOALLARME)==LOW);
  pir.Elabora(digitalRead(MOVIMENTO)==HIGH);
  // elabora uscite
  led.Elabora();
  sirena.Elabora();
  apricancello.Elabora();
  fari.Elabora();
  lanterna.Elabora();
  lampada.Elabora();
  
  // elabora funzioni
  ElaboraAntifurto();
  ElaboraCrepuscolare();
  if(Serial.available()) comm.ProcessaDatiSeriali(Serial.read());
  
}

// ingressi attivati
void PulsanteLuciClick() {
  lampada.Inverti();
}

void PulsanteLuciLongClick() {
  if(fari.isOn()) {
    fari.Off();
    lampada.Off();
  } else {
    fari.On();
    lampada.On();
  }
}

void PulsanteAntifurtoClick() {
  switch(modoantifurto) {
    case DISARMATO:
      setArmato();
      break;
    case ARMATO:
      setInCasa();
      break;
    case INCASA:
      setDisarmato();
      break;
  }
  DEBUG_PRINT(" modoalm=");
  DEBUG_PRINTLN(modoantifurto);
}

void ElaboraCrepuscolare() {
  static unsigned long int tultimocontrollo;
  if((millis() - tultimocontrollo) < INTERVALLOCONTROLLOCREPUSCOLARE) return;
  tultimocontrollo=millis();
  unsigned int val = 1024-analogRead(CREPUSCOLARE);
  if(!notte && (val<soglia_crepuscolare-isteresi_crepuscolare)) {notte=true; comm.Tx('D',0,0); lanterna.On(); return;}
  if(notte && (val>soglia_crepuscolare+isteresi_crepuscolare)) {notte=false; comm.Tx('E',0,0); lanterna.Off(); fari.Off(); lampada.Off(); return;}
}

void PirAttivato() {
  if(notte) {
    if(!fari.isOn()) fari.On(180000);
  }
}

void impostaled(int Ton, int Toff) {
  led.OndaQuadra(Ton,Toff);
}


void ElaboraAntifurto() {
  static bool statoprecedente;
  if(!sirena.isOn() && statoprecedente==true) {setDisarmato();};
  statoprecedente=sirena.isOn();
  if(modoantifurto==DISARMATO) return;
  if(digitalRead(MAGNETICI)==LOW) return;
  if(modoantifurto==ARMATO && digitalRead(CHIAVE)==HIGH) return;
  // se è già stato attivato esci
  if(!sirena.Completato()) return;
  sirena.On(tdurataallarme);
  comm.Tx('J',0,0);
}



void ElaboraComando(byte comando,byte *bytesricevuti,byte len) {
  switch(comando) {
    case 'A':
      setArmato();
      break;
    case 'C':
    case 'T':
      apricancello.OnOff(700,1000);
      break;
    case 'R':
      setInCasa();
      break;
    case 'O':
      setDisarmato();
      break;
    case 'Q':
      MemorizzaParametro(bytesricevuti,len);
      break;
    case 'N':
      setArmato();
      break;
    case 'H':
      //ping
      comm.Tx('G',1,"D");
      break;
    // movimento da cancello
    case 'B':
      PirAttivato();
      break;
  }
}

void MemorizzaParametro(byte *bytesricevuti,byte len) {
	switch(bytesricevuti[0]) {
		case 'S':
			soglia_crepuscolare=bytesricevuti[1]+bytesricevuti[2]*256;
			EEPROM.put(2,soglia_crepuscolare);
			break;
		case 'I':
			isteresi_crepuscolare=bytesricevuti[1];
			EEPROM.put(1,isteresi_crepuscolare);
			break;
		case 'D':
			tdurataallarme=bytesricevuti[1];
			EEPROM.put(0,tdurataallarme);
			break;
	}
}


void setDisarmato() {
  modoantifurto=DISARMATO;
  sirena.Off();
  comm.Tx('S',0,0);
  impostaled(30,1500);
}

// armato ma non in casa se le porte sono aperte e la chiave è on non entra in questo modo
void setArmato() {
  if(digitalRead(MAGNETICI)==HIGH) return;
  comm.Tx('R',0,0);
  modoantifurto=ARMATO;
  impostaled(200,200);
}

// armato in casa se le porte sono aperte non entra in questo modo
void setInCasa() {
  if(digitalRead(MAGNETICI)==HIGH) return;
  comm.Tx('U',0,0);
  modoantifurto=INCASA;
  impostaled(20,20);
}

