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
#define DEBUG
#ifdef DEBUG
 #define DEBUG_PRINT(x, ...)  Serial.print (x, ##__VA_ARGS__)
 #define DEBUG_PRINTLN(x, ...)  Serial.println (x, ##__VA_ARGS__)
#else
 #define DEBUG_PRINT(x, ...)
 #define DEBUG_PRINTLN(x, ...)  
#endif

// 6 relè 
#define FARI A0
#define BOH 3
#define LAMPADATAVOLO 4
#define LANTERNA 5
#define SIRENA 6
#define APRICANCELLO 7
// 7 ingressi
#define PULSANTELUCI 8
#define PULSANTEMODOALLARME 9
#define MAGNETICI 10
#define CHIAVE A1
#define MOVIMENTO A2
#define CREPUSCOLARE A3
// 1 uscita
#define LEDSTATO A4
#define TXENABLE A6

unsigned long tinizioallarme,tinizioapricancello,tinizioaccensionefari;
byte tdurataallarme; // in secondi

unsigned int soglia_crepuscolare;
byte isteresi_crepuscolare;
bool notte, faridapir;
typedef enum {DISARMATO, ARMATO, INCASA} modalitaantifurto;
modalitaantifurto modoantifurto; // 0=spento, 1=armato non in casa, 2=armato in casa

Antirimbalzo swLuci;
Antirimbalzo swAntifurto;

#define LEDPIN LED_BUILTIN
void setup() {
  Serial.begin(9600);
  digitalWrite(FARI,HIGH);
  digitalWrite(BOH,HIGH);
  digitalWrite(LAMPADATAVOLO,HIGH);
  digitalWrite(LANTERNA,HIGH);
  digitalWrite(SIRENA,HIGH);
  digitalWrite(APRICANCELLO,HIGH);
  digitalWrite(TXENABLE, LOW);

  pinMode(FARI, OUTPUT);
  pinMode(BOH, OUTPUT);
  pinMode(LAMPADATAVOLO, OUTPUT);
  pinMode(LANTERNA, OUTPUT);
  pinMode(SIRENA, OUTPUT);
  pinMode(APRICANCELLO, OUTPUT);
  //pinMode(LEDPIN, OUTPUT);
  pinMode(PULSANTELUCI, INPUT_PULLUP);
  pinMode(PULSANTEMODOALLARME, INPUT_PULLUP);
  pinMode(MAGNETICI, INPUT_PULLUP);
  pinMode(CHIAVE, INPUT_PULLUP);
  pinMode(MOVIMENTO, INPUT_PULLUP);
  pinMode(LEDSTATO, OUTPUT);
  pinMode(CREPUSCOLARE, INPUT_PULLUP);

  pinMode(TXENABLE, OUTPUT);
  setDisarmato();
  
  tdurataallarme=EEPROM.read(0)*1000;
  isteresi_crepuscolare=EEPROM.read(1);
  EEPROM.get(2,soglia_crepuscolare);
  notte=false;
  modoantifurto=DISARMATO;
  DEBUG_PRINT("dural=");
  DEBUG_PRINT(tdurataallarme);
  DEBUG_PRINT(" soglia=");
  DEBUG_PRINT(soglia_crepuscolare);
  DEBUG_PRINT(" ister=");
  DEBUG_PRINTLN(isteresi_crepuscolare);
  swLuci.tDurataClickLungo=350;
  swLuci.cbClickCorto=PulsanteLuciClick;
  swLuci.cbClickLungo=PulsanteLuciLongClick;
  swAntifurto.cbClickCorto=PulsanteAntifurtoClick;
}

void loop() {
  // put your main code here, to run repeatedly:
  swLuci.Elabora(digitalRead(PULSANTELUCI)==LOW);
  swAntifurto.Elabora(digitalRead(PULSANTEMODOALLARME)==LOW);
  LampeggioLED();
  ElaboraAntifurto();
  ElaboraCrepuscolare();
  ElaboraPir();
  if(Serial.available()) ProcessaDatiSeriali();
  if( (digitalRead(APRICANCELLO)==LOW) && ((millis()-tinizioapricancello)>700) ) {digitalWrite(APRICANCELLO,HIGH);};
  if( (digitalRead(FARI)==LOW) && faridapir && ((millis()-tinizioaccensionefari)>700) ) {digitalWrite(FARI,HIGH); faridapir=false;};
  
}


void PulsanteLuciClick() {
	if(digitalRead(LAMPADATAVOLO)==LOW) digitalWrite(LAMPADATAVOLO,HIGH); else digitalWrite(LAMPADATAVOLO,LOW);
}

void PulsanteLuciLongClick() {
  if(digitalRead(FARI)==LOW) {
    digitalWrite(FARI,HIGH);
    digitalWrite(LAMPADATAVOLO,HIGH);
  } else {
    digitalWrite(FARI,LOW);
    digitalWrite(LAMPADATAVOLO,LOW);
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
unsigned long  Tledoff, Tledon;
void impostaled(int Ton, int Toff) {
  Tledon=Ton;
  Tledoff=Toff;
}
void LampeggioLED() {
  static unsigned long tcambio=0,ison=false;
  unsigned long now=millis();
  unsigned long delta=now-tcambio;
  if(ison) {
    if(delta>=Tledon) {
      ison=false;
      tcambio=now;
      digitalWrite(LEDSTATO, LOW); 
    }
    
  } else {
    if(delta>=Tledoff) {
      ison=true;
      tcambio=now;
      digitalWrite(LEDSTATO, HIGH); 
    }
  }
}

void ElaboraAntifurto() {
  if(digitalRead(SIRENA)==LOW) if((millis()-tinizioallarme)>tdurataallarme) {digitalWrite(SIRENA, HIGH); Tx('K',0,0); modoantifurto=DISARMATO;};
  if(modoantifurto==DISARMATO) return;
  if(modoantifurto==ARMATO && digitalRead(CHIAVE)==HIGH) return;
  if(digitalRead(MAGNETICI)==LOW) return;
  // se è già stato attivato esci
  if(digitalRead(SIRENA)==LOW) return;
  digitalWrite(SIRENA, LOW);
  tinizioallarme=millis(); 
  Tx('J',0,0);
}


void Tx(char cmd, byte len, char* b) {
    byte sum=(byte)cmd+len;
    for(byte r=0;r<len;r++) sum+=b[r];
    digitalWrite(TXENABLE, HIGH);
    Serial.write('A');
    Serial.write(cmd);
    Serial.write(len);
    if(len>0) Serial.write(b,len);
    Serial.write(sum);
    while (!(UCSR0A & _BV(TXC0)));
    digitalWrite(TXENABLE, LOW);
}

#define A 0
#define L 1
#define D 2
#define C 3
#define S 4

void ProcessaDatiSeriali() {
  static byte numerobytesricevuti=0,bytesricevuti[6],prossimodato=A,lunghezza,comando,sum;
  static unsigned long tultimodatoricevuto;
  char c=Serial.read();
  /*
  DEBUG_PRINT("c=");
  DEBUG_PRINT(c,HEX);
  DEBUG_PRINT(" prox=");
  DEBUG_PRINTLN(prossimodato);
  */
  if(millis()-tultimodatoricevuto > 300) {prossimodato=A; };
  tultimodatoricevuto=millis();
  if(prossimodato==A && c=='A') {prossimodato=C; numerobytesricevuti=0; return;}
  if(prossimodato==C) {comando=c; prossimodato=L; sum=c; return;}
  if(prossimodato==L) {
    sum+=c;
    lunghezza=c;
    prossimodato=D; 
    if(lunghezza>6) prossimodato==A;
    if(lunghezza==0) {
      prossimodato=S;
    }
    /*
    DEBUG_PRINT("next D");
    DEBUG_PRINT(" l=");
    DEBUG_PRINTLN(lunghezza);
    */
    return;
  }
  if(prossimodato==D) {
    sum+=c;
    bytesricevuti[numerobytesricevuti++]=c;
    if(numerobytesricevuti==lunghezza) prossimodato=S;
    return;
  }
  if(prossimodato==S) {
    if(c==sum) {
      ElaboraComando(comando,bytesricevuti,lunghezza);
      prossimodato=A;
      //DEBUG_PRINTLN("next A");
    }
  }
}

void ElaboraComando(byte comando,byte *bytesricevuti,byte len) {
  switch(comando) {
    case 'A':
      setArmato();
      break;
    case 'C':
      digitalWrite(APRICANCELLO, LOW);
      tinizioapricancello=millis();
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
      Tx('G',1,"D");
      break;
    // movimento da cancello
    case 'B':
      if(notte && digitalRead(FARI)==HIGH) { digitalWrite(APRICANCELLO, LOW); tinizioaccensionefari=millis(); faridapir=true;}
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
  digitalWrite(SIRENA,HIGH);
  Tx('S',0,0);
  impostaled(30,1500);
}

// armato ma non in casa se le porte sono aperte e la chiave è on non entra in questo modo
void setArmato() {
  if(digitalRead(MAGNETICI)==HIGH) return;
  Tx('R',0,0);
  modoantifurto=ARMATO;
  impostaled(200,200);
}

// armato in casa se le porte sono aperte non entra in questo modo
void setInCasa() {
  if(digitalRead(MAGNETICI)==HIGH) return;
  Tx('U',0,0);
  modoantifurto=INCASA;
  impostaled(20,20);
}

void ElaboraCrepuscolare() {
  int val = 1024-analogRead(CREPUSCOLARE);
  if(!notte & val<soglia_crepuscolare-isteresi_crepuscolare) {notte=true; Tx('D',0,0); digitalWrite(LANTERNA,LOW); return;}
  if(notte & val>soglia_crepuscolare+isteresi_crepuscolare) {notte=false; Tx('E',0,0); digitalWrite(LANTERNA,HIGH); return;}
}

void ElaboraPir() {
  if(digitalRead(MOVIMENTO)==LOW) {
    if(notte) AccendiLuci();
  }
}

void AccendiLuci() {
  digitalWrite(FARI,LOW);
  digitalWrite(LAMPADATAVOLO,LOW);
}

