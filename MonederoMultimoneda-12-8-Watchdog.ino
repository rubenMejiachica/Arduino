//CONEXIONES
/*
PIN arduino uno------conexion
2 -------------------(INTERRUP)salida coin del lector multimoneda debe tener pull -up(Recordar que es una salida colector abierto)
3 --------------------(INTERRUP)boton abortar Pull-Down
8 -------Pulsador opcion 1 //agua Pull-Down
9 -------Pulsador opcion 2  //Jabón Pull-Down
A5 ------------------SCL I2C LCD
A4 ------------------SDA I2C LCD
A0 ------- salida digital 1 AGUA
A1 ------- salida digital 2 Jabon
**A2 ------- salida 3 (seguridad agua) //**se deja de usar 25 julio 2022
*************Display 7 seg-BCD**********
4----------------------a
5----------------------b
6----------------------c
7----------------------d

10--------------------COM digito izquierdo usando Transistor NPN
11--------------------COM digito derecho usando Transistor NPN

$$$$$$$$$$$MOdificacio sensado de jabon$$$$$$$$$$$$$$$$$$$$$
13--------------------entrada sensor nivel Pull-Down
A3--------------------Salida temporizada

%%%%%%%%%%%%%%%%%%LCD-SERVICIO%%%%%%%%%%%%%%%%

######################EEPROM#################
direccion donde se guardaerá el acumulado = 0
-se guardará cada 30 min para proteger la vida útil de la EEPROM (100 000 escrituras)
12-------------------------Boton reset EEPROM Pull-Down





*/
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <TimerOne.h>    
#include <EEPROM.h>
#include <YetAnotherPcInt.h>
#include <avr/wdt.h> // Incluir la librería que contiene el watchdog (wdt.h)


//Crear el objeto lcd  dirección  0x3F o 0x27 y 16 columnas x 2 filas
LiquidCrystal_I2C lcd(0x27,16,2);  //pantalla usuario
LiquidCrystal_I2C lcd2(0x26,16,2);  //pantalla dinero total

//******************************************************************
//*******************seccion para cada caracter de la barra de carga (5 columnas)
//These 5 arrays paint the bars that go across the screen.  
byte zero[] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};
byte one[] = {
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000
};

byte two[] = {
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000
};

byte three[] = {
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100
};

byte four[] = {
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110
};

byte five[] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};
byte o2[8] = {  //ó con tilde
 B00010,
 B00100,
 B01110,
 B10001,
 B10001,
 B10001,
 B01110,
 B00000,
};
byte e2[8] = {  //é con tilde
 B00010,
 B00100,
 B01110,
 B10001,
 B11111,
 B10000,
 B01110,
 B00000
};
//******************************************************************
//******************************************************************

//Constantes para los pines.
const int SignalPin = 2;

//Constantes para las monedas. (PULSOS)
const int mil = 1;
const int quinientos = 2;//cualquiera de las dos
const int docientos1 = 3; //pequeña
const int docientos2 = 4; //grande
const int cien1 = 5; //pequeña
const int cien2 = 6; //grande
const int precio=1000; //precio de los servicios
const byte tiempo_secuencia=90; //segundos de la secuencia
const byte eeAddress=0;//direccion donde se guardara el acumulado 0-3 (Nota: la eprom tiene 1024 bytes)
const int TIME_EEPROM=1800; // 60 seg x 30min =1800 para que escriba en eeprom cada 30min
const byte time_nivel_jabon=3; //Tiempo para actival la salida una vez decaiga el sensor nivel jabon


//Variables.
volatile int pulso = 0;
//volatile unsigned long MillisUltPulso = 0;
unsigned long MillisUltPulso = 0;
int PulsosAcum = 0;
unsigned int CreditoAcum = 0;
int MaxTimePulse = 200;
byte opcion=0; //Variable para realizar la acción segun la opción elegida por el usuario
byte tiempo=tiempo_secuencia ;// Tiempo que dura cada accion en segundos
byte tiempo2=2; //tiempo para mostrar precio (SI Aprietan una opcion sin el credito suficiente)
//byte tiempo_seguridad=2; //tiempo para retardar el cerrado del agua
int tiempo_EEPROM=TIME_EEPROM; //tiempo en segundos para guardar en la eeprom 
byte flag_tiempo=0; //Bandera para la interrupcion de timer

bool conmutador=0;//conmutador para el display 7 seg
byte var=0;
byte unidad=0;
byte decena=0;
unsigned long gran_total=0; //almacena el gran total en RAM
//unsigned long gran_total_EEPROM=0; //almacena el gran total en EEPROM
byte flag_nivel_jabon=0;//bandera que se activa una vez se recibe la interrupcion por nivel de jabon
byte tiempo_nivel_jabon=time_nivel_jabon; //igualo el tiempo para la salida del nivel de jabon a la constante antes creada


void setup() {
  wdt_disable(); // Desactivar el watchdog mientras se configura, para que no se resetee
  wdt_enable(WDTO_8S); // Configurar watchdog a 8 segundos

  // Inicializar el LCD 1
  lcd.init();
  //LIMPIAR LCD
  lcd.clear();
  //Encender la luz de fondo.
  lcd.backlight();
  imprimir_todo(); //Funcion creada para imprimir credito y opcion

    // Inicializar el LCD 2
  lcd2.init();
  //LIMPIAR LCD
  lcd2.clear();
  //Encender la luz de fondo.
  lcd2.backlight();

  //##################################LEEMOS LA EEPROM para cargar gran_total
EEPROM.get(eeAddress,gran_total);
pinMode(12,INPUT);//boton servicio para resetear EEPROM
pantalla_servicio();//imprimimos el total en la pantalla de servicio


  //***********************************
  //**Caracteres para la barra de carga
  lcd.createChar(0, zero);
  lcd.createChar(1, one);
  lcd.createChar(2, two);
  lcd.createChar(3, three);
  lcd.createChar(4, four);
  lcd.createChar(5, five);
  lcd.createChar(6, o2);
  lcd.createChar(7, e2);// é con tilde
  //***********************************

  //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  //TIMER1
  Timer1.initialize(1000000);      //Configura el TIMER en 1 Segundo
  Timer1.attachInterrupt(interrupt_Temporizador) ; //Configura la interrupción del Timer 1
   //%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  

//Configuracion de botones
pinMode(8,INPUT);//pin botón opcion 1
pinMode(9,INPUT);//pin botón opcion 2

//configuracion de salidas
pinMode(14,OUTPUT);//salida 1
pinMode(15,OUTPUT);//salida 2
//pinMode(16,OUTPUT);//salida 3 (seguridad agua)
//---------------Display 7 segmentos------------
pinMode(4,OUTPUT);//BCD a
pinMode(5,OUTPUT);//BCD b
pinMode(6,OUTPUT);//BCD c
pinMode(7,OUTPUT);//BCD d

pinMode(10,OUTPUT);//comun display izquierdo 
pinMode(11,OUTPUT);//comun display derecho

pinMode(13,INPUT);//sensor nivel jabón
pinMode(17,OUTPUT);//Salida temporizada jabon

reset_display();// apaga el display 7 seg
//---------------------------------------------
// Inicializamos la comunicacion serial, para ver los resultados en el monitor.
Serial.begin(9600);

//Agregamos la interrupcion con el pin indicado.
attachInterrupt(digitalPinToInterrupt(SignalPin), coinInterrupt, FALLING);//Interrupcion pulsos monedero
attachInterrupt(digitalPinToInterrupt(3), abortInterrupt, RISING);//Interrupcion boton abortar

//********************************************
//Interrupciones PCINT*************************
//********************************************
//12-----Interrupcion Reset EEPROM
pinMode(12, INPUT);
PcInt::attachInterrupt(12, pinChanged_12, RISING);
//13-----Interrupcion nivel jabón
pinMode(13, INPUT);
PcInt::attachInterrupt(13, pinChanged_13, RISING);


    
}

// Interrupcion Monedero
void coinInterrupt(){
 
  // Cada vez que insertamos una moneda valida, incrementamos el contador de monedas y encendemos la variable de control,
  pulso++;
  MillisUltPulso = millis();
  Serial.println("Interrupcion");
   
}

//Interrupción externa boton parar, reset EEPROM
void abortInterrupt()
{
  /*if(digitalRead(12)==HIGH)//Boton reset EEPROM
  {
    reset_EEPROM();//Funcion para resetear la eeprom
   }
  else if(digitalRead(13)==HIGH)
  {
    flag_nivel_jabon=1;//activa la bandera de sensor nivel jabon
    }
  else*/
    opcion=0; //pone opcion en 0 para que termine el ciclo 

}

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%TIMER1 de 1 seg
void interrupt_Temporizador(void)
{
  tiempo_EEPROM--; // reduce el tiempo para escribir en la eprom cuando llegue a 0 (a los 30min)
   Serial.print("GRAN Total: $$$ ");
   Serial.println(gran_total);
  switch (flag_tiempo)
  {
    case 1: //agua
      tiempo--; //reduce tiempo de secuencia
    break;
    case 2:// Jabon
      tiempo--;//reduce tiempo de secuencia
    break;
    case 3:
      tiempo2--; //reduce tiempo de muestra precio
    break;
  /*  case 4:
      tiempo_seguridad--; //reduce tiempo de seguridad
    break;
    */
  }

  if(flag_nivel_jabon==1)//si se activa el sensor de nivel jabon
    tiempo_nivel_jabon--;
}
//*********************************************************
//----------------------------Interrupciones pines 12 y 13
//**********************************************************

//**********************Interrupcion Reset EEPROM
void pinChanged_12(void)
{
  reset_EEPROM();//Funcion para resetear la eeprom
  }
//*********************Interrupcion Nivel jabon
void pinChanged_13(void)
{
  flag_nivel_jabon=1;//activa la bandera de sensor nivel jabon
  }

//*********************************************************
void loop() {
wdt_reset(); // Actualizar el watchdog para que no produzca un reinicio

//Calculamos los milisegundos de la ultima ejecusion menos el ultimo tiempo que se genero un pulso.
unsigned long lastTime = millis() - MillisUltPulso;

//Validamos si hay algun puslo, si es asi tambien se valida que el ultimo tiempo asignado sea mayor a la cantidad de milisegundos establecidos.
if((pulso > 0) && (lastTime >= MaxTimePulse)){  //Han pasado mas de 200ms sin una interrupcion 

    //La cantidad de creditos es el contador y acumulador de pulsos, hasta que se cumpla alguna condicion.
    PulsosAcum = pulso;
    pulso = 0;
    Serial.print("Pulses: ");
    Serial.print(PulsosAcum);
    Serial.print(" LastTime: ");
    Serial.print(lastTime);
    Serial.print(" LastPulse: ");
    Serial.println(MillisUltPulso);    
}

//Validamos la moneda depositada.
switch (PulsosAcum){

  case mil:
    PulsosAcum = 0;
    Serial.println("Moneda depositada de $ 1000");
    CreditoAcum += 1000;
    gran_total+=1000; //va sumando al gran total
    Serial.print("Credito Total: $ ");
    Serial.print(CreditoAcum);
    Serial.println(".00");
        Serial.print("GRAN Total: $$$ ");
    Serial.println(gran_total);

    imprimir_credito();//Funcion para imprimir credito 
     pantalla_servicio();
    
    break;

  case quinientos:
    PulsosAcum = 0;
    Serial.println("Moneda depositada de $ 500");
    CreditoAcum += 500;
    gran_total+=500; //va sumando al gran total
    Serial.print("Credito Total: $ ");
    Serial.print(CreditoAcum);
    Serial.println(".00");
        Serial.print("GRAN Total: $$$ ");
    Serial.println(gran_total);
    imprimir_credito();//Funcion para imprimir credito 
     pantalla_servicio();
    break;
    
  case docientos1:

    PulsosAcum = 0;
    Serial.println("Moneda depositada de $ 200");
    CreditoAcum += 200;
    gran_total+=200; //va sumando al gran total
    Serial.print("Credito Total: $ ");
    Serial.print(CreditoAcum);
    Serial.println(".00");
        Serial.print("GRAN Total: $$$ ");
    Serial.println(gran_total);
    imprimir_credito();//Funcion para imprimir credito 
     pantalla_servicio();//print on service screen
    break;

  case docientos2:

    PulsosAcum = 0;
    Serial.println("Moneda depositada de $ 100");
    CreditoAcum += 200;
    gran_total+=200; //va sumando al gran total
    Serial.print("Credito Total: $ ");
    Serial.print(CreditoAcum);
    Serial.println(".00");
        Serial.print("GRAN Total: $$$ ");
    Serial.println(gran_total);
    imprimir_credito();//Funcion para imprimir credito 
     pantalla_servicio();
    break;

case cien1:

    PulsosAcum = 0;
    Serial.println("Moneda depositada de $ 50");
    CreditoAcum += 100;
    gran_total+=100; //va sumando al gran total
    Serial.print("Credito Total: $ ");
    Serial.print(CreditoAcum);
    Serial.println(".00");
        Serial.print("GRAN Total: $$$ ");
    Serial.println(gran_total);
    imprimir_credito();//Funcion para imprimir credito 
     pantalla_servicio();
    break;

case cien2:

    PulsosAcum = 0;
    Serial.println("Moneda depositada de $ 50");
    CreditoAcum += 100;
    gran_total+=100; //va sumando al gran total
    Serial.print("Credito Total: $ ");
    Serial.print(CreditoAcum);
    Serial.println(".00");
    Serial.print("GRAN Total: $$$ ");
    Serial.println(gran_total);
    imprimir_credito();//Funcion para imprimir credito 
    pantalla_servicio();
    break;


  }
  //***************************************************************---
  //***************************************************************--
      //Botones para las opciones-*-*-*-*-*-*-*-*-*-*-*-*-**-*-* solo muestra el valor pues no hay credito suficiente
    if (digitalRead(8)==HIGH)
     { 
        if (CreditoAcum<precio) //si el credito es menor que el precio, solo muestra el nombre de la opcion y el precio por 1.5 s
        {
        opcion=1;
        imprimir_opcion();//Funcion para imprimir opcion temporalmente
        }
        else
          opcion=1;
     }
    else if (digitalRead(9)==HIGH)
     { 

       if (CreditoAcum<precio)//si el credito es menor que el valor, solo muestra el nombre de la opcion y el precio por 1.5 s
       {
        opcion = 2;
        imprimir_opcion();//Funcion para imprimir opcion temporalmente
       }
       else
        opcion=2;
     }

   //Arranque*********************************************

      if(opcion!=0 && CreditoAcum>=precio)
      {
        switch (opcion)
        {
          case 1:
          digitalWrite(14,HIGH); //Agua
          //digitalWrite(16,HIGH); //seguridad agua
          flag_tiempo=1;
          break;
          case 2:
          digitalWrite(15,HIGH);//jabon
          flag_tiempo=2;
          break;
        }
        CreditoAcum=CreditoAcum-precio;
        imprimir_accion();
        while (tiempo>0)  //Retardo para el tiempo
        { 
          wdt_reset(); // Actualizar el watchdog para que no produzca un reinicio

          if(opcion!=0)//constante mente pregunta por opcion para que en caso de boton reset, opcion se torna cero y quiebra el conteo
          {
          //********************************************************************
          imprimir_tiempo();
          dibujo_display();
          //*********************** IMPRIMIR Barra de carga*******************************
          updateProgressBar(tiempo, tiempo_secuencia, 1);   //This line calls the subroutine that displays the progress bar.  The 3 arguments are the current count, the total count and the line you want to print on.
          //*******************************************************************
              //dentro del ciclo de agua o jabon, también se debe preguntar por la interrupciondel nivel de jabon***
              if(flag_nivel_jabon==1)
                fun_nivel_jabon();
          //delay(1000);
          //tiempo--;

        //  imprimir_tiempo();
          }
          else
            break; //si opcion cambia a 0 (boton abortar) detiene el while

          
        }

        
       //Una vez terminado el tiempo Reset a todo menos a credito
       reset(); 
       //reset_seguridad();
       
      }

//#################################################Pregunta si se cumplio el momento de escribir en la EEPROM
    if(tiempo_EEPROM<=0)
      escribir_EEPROM();
//###############################################Nivel Jabon
    if(flag_nivel_jabon==1)
        fun_nivel_jabon();
   
     
   
  
}
//OTRAS FUNCIONES*--*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*--*-*-*-*--*
//Funcion para imprimir
void imprimir_todo()
{
    lcd.clear(); //limpiar pantalla
    lcd.setCursor(3,0);//col 0, fil
    lcd.print("Cr");
    lcd.setCursor(5,0);//col 0, fil
    lcd.write(7);//é
    lcd.setCursor(6,0);//col 0, fil
    lcd.print("dito: ");
    lcd.setCursor(6,1);//col 0, fil
    lcd.print(CreditoAcum);

}
//NO USADA
void imprimir_credito(){ 
    lcd.setCursor(0,1);//col 0, fil
    lcd.print("                ");//Limpia
    lcd.setCursor(6,1);//col 0, fil
    lcd.print(CreditoAcum);
  }
  
void imprimir_opcion(){  //Imprime las opciones temporalmente cuando no se alcanza el credito necesario
    lcd.clear(); //limpiar pantalla
    flag_tiempo=3;
    switch(opcion)
    {
      case 1:
        
        lcd.setCursor(3,0);//col 0, fil
        lcd.print("Agua ");//Limpia
        lcd.setCursor(2,1);//col 0, fil
        lcd.print("Precio: ");
        lcd.setCursor(10,1);//col 0, fil
        lcd.print(precio);
        break;
      case 2:
      /*
        lcd.setCursor(3,0);//col 0, fil
        lcd.print("JABON1:");//Limpia
        lcd.setCursor(2,1);//col 0, fil
        lcd.print("Precio: ");//Limpia
        lcd.setCursor(10,1);//col 0, fil
        lcd.print(precio);
        */
        lcd.setCursor(3,0);//col 0, fil
        lcd.print("Jab");//Limpia
        lcd.setCursor(6,0);//col 0, fil
        lcd.write(6);// el 6 es ó
        lcd.setCursor(7,0);//col 0, fil
        lcd.print("n ");// el 6 es ó
        lcd.setCursor(2,1);//col 0, fil
        lcd.print("Precio: ");//Limpia
        lcd.setCursor(10,1);//col 0, fil
        lcd.print(precio);
      break;
    }
    //delay(1500);// espera 1.5 seg
    while(tiempo2>0) //espera 2 seg
    {
      
      lcd.setCursor(15,0);//col 0, fil
      lcd.print(" ");//Comodín si no se pone, se queda en el ciclo
    }
    opcion=0; //reset a opcion
    tiempo2=2; //reset a tiempo 2
    flag_tiempo=0;//reset a bandera tiempo
    imprimir_todo();//Vuelve a la pantalla ppal
  }

void imprimir_tiempo(){

  if (tiempo<10)
  {
    lcd.setCursor(12,0);
    lcd.print(" ");
  }
    lcd.setCursor(11,0);
    lcd.print(tiempo);
  }

  
  
void imprimir_accion()
{
    for(byte i=0; i <= 16; i++)  //for para llenar la barra de carga
    {
    lcd.setCursor(i,1);//col,fil
    lcd.write(5);
    }
    switch (opcion)
    {
      case 1:
         //limpiar pantalla
        lcd.setCursor(0,0);//col 0, fil
        lcd.print("                ");
        lcd.setCursor(3,0);//col 0, fil
        lcd.print("Agua ");

      break;
      case 2:
        //limpiar pantalla
        lcd.setCursor(0,0);//col 0, fil
        lcd.print("                ");
        lcd.setCursor(3,0);//col 0, fil
        lcd.print("Jab");//Limpia
        lcd.setCursor(6,0);//col 0, fil
        lcd.write(6);// el 6 es ó
        lcd.setCursor(7,0);//col 0, fil
        lcd.print("n ");// el 6 es ó

      break;

    }
}

void pantalla_servicio()//Funcion para imprimir el crédito acumulado en la segunda pantalla
{
        lcd2.setCursor(0,0);//col 0, fil
        lcd2.print("                ");
        lcd2.setCursor(3,0);//col 0, fil
        lcd2.print("$ ");//pesos
        lcd2.setCursor(5,0);//col 0, fil
        lcd2.print(gran_total);//Limpia
}

void dibujo_display()
{
    decena=tiempo/10;
    unidad=tiempo%10;
  //============================ 
  //Multiplexación  
    if (conmutador == 0) {    // hace la multiplexacion conmutando entre los dos 7seg  izq y der
      digitalWrite(10, 0);    // apaga el izquierdo
      digitalWrite(11, 1);    // enciende el derecho
      var=unidad;             // iguala la variable que escribe el numero en el 7seg al valor de la unidad
      conmutador=1;           // cambia el conmutador para que en el siguiente ciclo cumpla la otra condicion
      
    }
    else{
      digitalWrite(10, 1);    // enciende el izquierdo
      digitalWrite(11, 0);    // apaga el derecho
      var=decena;               // iguala la variable que escribe el numero en el 7seg al valor de la decena
      conmutador=0;             // cambia el conmutador para que en el siguiente ciclo cumpla la otra condicion
      
    }
//=============================
//=============================
//   DIBUJADO

    switch (var) {
    case 1:                 //escribe en el 7seg el numero 1
      digitalWrite(4, 1);//BCD a
      digitalWrite(5, 0);//BCD b
      digitalWrite(6, 0);//BCD c
      digitalWrite(7, 0);//BCD d

     break;
    case 2:                //escribe en el 7seg el numero 2
      digitalWrite(4, 0);//BCD a
      digitalWrite(5, 1);//BCD b
      digitalWrite(6, 0);//BCD c
      digitalWrite(7, 0);//BCD d
     break;      
    case 3:               //escribe en el 7seg el numero 3
      digitalWrite(4, 1);//BCD a
      digitalWrite(5, 1);//BCD b
      digitalWrite(6, 0);//BCD c
      digitalWrite(7, 0);//BCD d
      break;
    case 4:               //escribe en el 7seg el numero 4
      digitalWrite(4, 0);//BCD a
      digitalWrite(5, 0);//BCD b
      digitalWrite(6, 1);//BCD c
      digitalWrite(7, 0);//BCD d
     break;
    case 5:               //escribe en el 7seg el numero 5
      digitalWrite(4, 1);//BCD a
      digitalWrite(5, 0);//BCD b
      digitalWrite(6, 1);//BCD c
      digitalWrite(7, 0);//BCD d
     break;
    case 6:               //escribe en el 7seg el numero 6
      digitalWrite(4, 0);//BCD a
      digitalWrite(5, 1);//BCD b
      digitalWrite(6, 1);//BCD c
      digitalWrite(7, 0);//BCD d
     break;
    case 7:              //escribe en el 7seg el numero 7
      digitalWrite(4, 1);//BCD a
      digitalWrite(5, 1);//BCD b
      digitalWrite(6, 1);//BCD c
      digitalWrite(7, 0);//BCD d
     break;
    case 8:              //escribe en el 7seg el numero 8
      digitalWrite(4, 0);//BCD a
      digitalWrite(5, 0);//BCD b
      digitalWrite(6, 0);//BCD c
      digitalWrite(7, 1);//BCD d
      break;
    case 9:               //escribe en el 7seg el numero 9
      digitalWrite(4, 1);//BCD a
      digitalWrite(5, 0);//BCD b
      digitalWrite(6, 0);//BCD c
      digitalWrite(7, 1);//BCD d
     break;
    case 0:                //escribe en el 7seg el numero 0
      digitalWrite(4, 0);//BCD a
      digitalWrite(5, 0);//BCD b
      digitalWrite(6, 0);//BCD c
      digitalWrite(7, 0);//BCD d
      break;
           
    default: 
      digitalWrite(4, 0);//BCD a
      digitalWrite(5, 0);//BCD b
      digitalWrite(6, 0);//BCD c
      digitalWrite(7, 0);//BCD d
      
  }
  //=============================
}
void reset_display() //Apaga el display 7 seg
{
      digitalWrite(4, 0);//BCD a
      digitalWrite(5, 0);//BCD b
      digitalWrite(6, 0);//BCD c
      digitalWrite(7, 0);//BCD d

      digitalWrite(10, 0);    //apaga el derecho
      digitalWrite(11, 0);    // apaga el izquierdo
}

void reset()
{
  digitalWrite(14,LOW);//apaga salidas agua y jabón
  digitalWrite(15,LOW);
  tiempo=tiempo_secuencia;//reset a tiempo de secuencia
  opcion =0; //reset a opcion
  flag_tiempo=0;//reset a bandera tiempo
  imprimir_todo();
  reset_display();
  
}
/*
void reset_seguridad()
{
  flag_tiempo=4;
  while(tiempo_seguridad>0)
  {
     lcd.setCursor(15,0);//col 0, fil
     lcd.print(" ");//Comodín si no se pone, se queda en el ciclo
  }
  digitalWrite(16,LOW); //apaga el relé de la válvula agua
  flag_tiempo=0; //reset a la bandera tiempo
  tiempo_seguridad=2; //reset al tiempo de seguridad
}


*/

 void escribir_EEPROM()//Funcion para escribir en la EEPROM
 {
    EEPROM.put(eeAddress,gran_total);//set the eeprom dir 0
    tiempo_EEPROM=TIME_EEPROM; //reset tiempo_EEPROM
     Serial.print("LOAD EEPROM----------------------------------");
    
 }
 void reset_EEPROM()
 {
    
    gran_total=0;
    //CreditoAcum=0;
    EEPROM.put(0,gran_total);//Reset EEPROM
 }

 void fun_nivel_jabon()
 {
    digitalWrite(17,HIGH);//pone la salida nivel jabon en alto
    if(tiempo_nivel_jabon<=0)//una vez cumplido el tiempo
    {
      digitalWrite(17,LOW);//pone la salida nivel jabon en bajo
      flag_nivel_jabon=0;//reset a bandera nivel jabon
      tiempo_nivel_jabon=time_nivel_jabon;//reset al tiempo de la salida nivel jabon
    }
      
 }
//******************************************************************************
//*****************Funcion para la barra de carga******************************
/*
 * This is the method that does all the work on the progress bar.
 * Please feel free to use this in your own code.
 * @param count = the current number in the count progress
 * @param totalCount = the total number to count to
 * @param lineToPrintOn = the line of the LCD to print on.
 * 
 * Because I am using a 16 x 2 display, I have 16 characters.  Each character has 5 sections.  Therefore, I need to declare the number 80.0.
 * If you had a 20 x 4 display, you would have 20 x 5 = 100 columns.  Therefore you would change the 80.0 to 100.0
 * You MUST have the .0 in the number.  If not, it will be treated as an int and will not calculate correctly
 * 
 * The factor is the totalCount/divided by the number of columns.
 * The percentage is the count divided by the factor (so for 80 columns, this will give you a number between 0 and 80)
 * the number gives you the character number (so for a 16 x 2 display, this will be between 0 and 16)
 * the remainder gives you the part character number, so returns a number between 0 and 4
 * 
 * Based on the number and remainder values, the appropriate character is drawn on the screen to show progress.
 * This only works for a steady count up or down.  This will not work as is for fluctuating values, such as from an analogue input.
 */
 void updateProgressBar(unsigned long count, unsigned long totalCount, int lineToPrintOn)
 {
    double factor = totalCount/80.0;          //See note above!
    int percent = (count+1)/factor;
    int number = percent/5;
    int remainder = percent%5;
    if(number > 0)
    {
       lcd.setCursor(number-1,lineToPrintOn);
       lcd.write(5);
    }
   
       lcd.setCursor(number,lineToPrintOn);
       lcd.write(remainder);   
 }
//******************************************************************************
