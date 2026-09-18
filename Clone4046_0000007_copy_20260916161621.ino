// ============================================================
// ATtiny85 - Synchronisation moteur NBTV
// Reproduction simplifiée du comportement du CD4046
//
// PB0 = SYNC NBTV (SYNCH)
// PB2 = OPTO (PULSEDISC)
// PB1 = commande moteur 3 états
//
// SYNC en premier  -> HIGH jusqu'à OPTO
// OPTO en premier  -> LOW  jusqu'à SYNC
// Deuxième impulsion -> Hi-Z
//
// Le réglage de vitesse de base est effectué par RV1.
// L'ATtiny85 ne fait que les corrections de position.
// Remarque : l'ATtiny 85 est alimenté de manière un peu particulière depuis le 15V : ene résistance, une zener de 5.1V puis une LED rouge dont la cathode est raccordée au GND
// L'ATtiny 85 a ses pins d'alimentation connectées sur la zener et la tension de LED est là pour réhausser un peu le niveau d'attaque vers le mosfet
// ============================================================

const byte SYNC_PIN  = PB0;
const byte MOTOR_PIN = PB1;
const byte OPTO_PIN  = PB2;

// Temps maximum pendant lequel une correction peut rester active.
// 6000 us = largement supérieur à l'écart normal entre deux impulsions.
// C'est uniquement une sécurité si une impulsion disparaît.
const unsigned long MAX_CORRECTION_US = 6000;

volatile byte previousInputs = 0;

volatile byte correctionState = 0;
// 0 = aucune correction
// 1 = SYNC arrivé en premier -> HIGH
// 2 = OPTO arrivé en premier -> LOW

volatile unsigned long correctionStart = 0;


// ------------------------------------------------------------
// Commande 3 états sur PB1
// ------------------------------------------------------------

void motorHigh()
{
  PORTB |= _BV(MOTOR_PIN);
  DDRB  |= _BV(MOTOR_PIN);
}

void motorLow()
{
  PORTB &= ~_BV(MOTOR_PIN);
  DDRB  |= _BV(MOTOR_PIN);
}

void motorHiZ()
{
  PORTB &= ~_BV(MOTOR_PIN);
  DDRB  &= ~_BV(MOTOR_PIN);
}


// ------------------------------------------------------------
// Interruption Pin Change
// ------------------------------------------------------------

ISR(PCINT0_vect)
{
  byte currentInputs = PINB;

  // On ne s'intéresse qu'aux fronts montants
  byte rising = currentInputs & ~previousInputs;

  previousInputs = currentInputs;

  bool syncRising = rising & _BV(SYNC_PIN);
  bool optoRising = rising & _BV(OPTO_PIN);


  // ----------------------------------------------------------
  // Les deux fronts arrivent pratiquement simultanément
  // -> aucune correction
  // ----------------------------------------------------------

  if (syncRising && optoRising)
  {
    motorHiZ();
    correctionState = 0;
    return;
  }


  // ----------------------------------------------------------
  // SYNC arrive
  // ----------------------------------------------------------

  if (syncRising)
  {
    if (correctionState == 0)
    {
      // SYNC en premier :
      // l'ATtiny85 passe sa sortie à HIGH
      motorHigh();

      correctionState = 1;
      correctionStart = micros();
    }
    else if (correctionState == 2)
    {
      // OPTO était arrivé en premier :
      // la paire d'impulsions est terminée
      motorHiZ();

      correctionState = 0;
    }

    return;
  }


  // ----------------------------------------------------------
  // OPTO arrive
  // ----------------------------------------------------------

  if (optoRising)
  {
    if (correctionState == 0)
    {
      // OPTO en premier :
      // l'ATtiny85 passe sa sortie à LOW
      motorLow();

      correctionState = 2;
      correctionStart = micros();
    }
    else if (correctionState == 1)
    {
      // SYNC était arrivé en premier :
      // la paire d'impulsions est terminée
      motorHiZ();

      correctionState = 0;
    }

    return;
  }
}


// ------------------------------------------------------------
// SETUP
// ------------------------------------------------------------

void setup()
{
  // Entrées
  DDRB &= ~_BV(SYNC_PIN);
  DDRB &= ~_BV(OPTO_PIN);

  // Pas de pull-up interne
  PORTB &= ~_BV(SYNC_PIN);
  PORTB &= ~_BV(OPTO_PIN);

  // Sortie moteur initialement flottante
  motorHiZ();

  // ----------------------------------------------------------
  // Démarrage moteur
  // ----------------------------------------------------------

  motorHigh();
  delay(4000);  // A cause de l'inertie du moteur

  motorHiZ();

  // Etat actuel des entrées
  previousInputs = PINB;

  correctionState = 0;

  // ----------------------------------------------------------
  // Activation des interruptions Pin Change
  // PB0 et PB2
  // ----------------------------------------------------------

  PCMSK |= _BV(PCINT0);   // PB0
  PCMSK |= _BV(PCINT2);   // PB2

  GIMSK |= _BV(PCIE);

  sei();
}


// ------------------------------------------------------------
// LOOP
// ------------------------------------------------------------

void loop()
{
  // ----------------------------------------------------------
  // Sécurité :
  // si une impulsion manque, on ne laisse pas PB1 bloqué
  // indéfiniment en HIGH ou LOW.
  // ----------------------------------------------------------

  if (correctionState != 0)
  {
    unsigned long elapsed = micros() - correctionStart;

    if (elapsed > MAX_CORRECTION_US)
    {
      noInterrupts();

      motorHiZ();
      correctionState = 0;
      previousInputs = PINB;

      interrupts();
    }
  }
}