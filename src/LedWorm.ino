/* Draws a small rainbow "worm" on an APA102 LED strip.
 *
 * The direction and speed of the worm can be controlled
 * by a potentiometer.
 *
 * The direction and speed of the rainbow colors shift
 * is also controlled by a potentiometer.
 *
 * The end points of the pots will automatically be calibrated.
 *
 * The code is setup to work with the Arduino Micro (Leonardo compatible) board.
 */ 

// Uses the FastLED (github.com/FastLED) lib to control the LEDs.
#include "FastLED.h"
#include "fastpin_avr.h"

// Number of LEDs in the strip
#define NUM_LEDS 144

// The LED array.
CRGB leds[NUM_LEDS];

// Analog pins for the pots.
#define COLOR_POT_PIN 0
#define SPEED_POT_PIN 1

// Fixed size number of LEDs in the rainbow.
#define NUM_RAINBOW_LEDS 12

// LED intensity arrays.
uint8_t green[NUM_RAINBOW_LEDS];
uint8_t   red[NUM_RAINBOW_LEDS];
uint8_t  blue[NUM_RAINBOW_LEDS];


void setup() {

  // Full intensity (255) is too much when testing at the desktop. Use a lower value.
#define MAX_INTENSITY 30
// This seems to give decent transitions between the colors.
#define MEDIUM_INTENSITY (MAX_INTENSITY / 4)

  // Setup a color progression so that the colors mix accordingly:
  // G: -=====-_____
  // R: ____-=====-_
  // B: ==-     -===
  //
  // =: Max intensity
  // -: Medium intensity
  // _: No instensity

  uint8_t intensity_progression[NUM_RAINBOW_LEDS] = {
    MEDIUM_INTENSITY,
    MAX_INTENSITY,
    MAX_INTENSITY,
    MAX_INTENSITY,
    MAX_INTENSITY,
    MAX_INTENSITY,
    MEDIUM_INTENSITY,
    0,
    0,
    0,
    0,
    0
  };

  // The colors are shifted 4 steps to get the mixing above.
  uint8_t shift = 4;

  // Add the intensity curve and let the intensity be shifted between the colors.
  for (uint8_t i = 0; i < NUM_RAINBOW_LEDS; i++) {
    green[(i + 0 * shift) % NUM_RAINBOW_LEDS] = intensity_progression[i];
      red[(i + 1 * shift) % NUM_RAINBOW_LEDS] = intensity_progression[i];
     blue[(i + 2 * shift) % NUM_RAINBOW_LEDS] = intensity_progression[i];
  }

  // Delay from FastLED examples.
  delay(2000);

  // Uses Hardware SPI using the Arduino Micro pins labeled MOSI and SCK.
  FastLED.addLeds<APA102,  SPI_DATA, SPI_CLOCK, BGR>(leds, NUM_LEDS);

  // Start by turning off all LEDs.
  for(int whiteLed = 0; whiteLed < NUM_LEDS; whiteLed = whiteLed + 1) {
    leds[whiteLed] = CRGB::Black;
  }
}

class AnalogReader {
  // The analog pin to read.
  const uint8_t _pin;
  // The top of the span that will be returned [0, top].
  const int _top;
  // The range of the seen values.
  int _min;
  int _max;

 public:
  AnalogReader(uint8_t pin, int top) : _pin(pin), _top(top), _min(1023), _max(0) { }

  /* Reads the analog value from the given analog pin [0, 1023],
   * and converts it to a value between [0, top].
   *
   * The function uses the read value to auto-calibrate the seen range
   * and then maps the value within the range [min, max] to a value between [0, top].
   *
   * Returns a value between [0, top].
   */
  int get_value() {
    int value = analogRead(_pin);

    // Record new min and max values.
    _min = (value < _min) ? value : _min;
    _max = (value > _max) ? value : _max;

    int range = _max - _min;

    // How far within the range the value lies.
    int relative = value - _min;

    if (range == 0) {
      value = _top;
    } else {
      // Use 32 bits since <max from analogRead: 1023> * <max as top: 65355>
      // is larger than a int/uint16_t.
      value = uint32_t(_top) * relative / range;
    }

    return value;
  }
};

// Convert the read values from the pots into suitable direction and speed.
void set_direction_and_speed(int value, int& direction, int& speed) {
  // [0, 1023] -> [-512, 511]
  value -= 512;

  // Apply deadband to help center the pots.
  if (abs(value) < 32) {
    value = 0;
  }

  // The directions depend on how the pots are connected and the LEDs are positioned.
  // The directions are hard-coded for now.
  if (value < 0) {
    direction =  1;
  } else if (value > 0) {
    direction = -1;
  } else {
    direction =  0;
  }

  speed = abs(value);
}

// Setup auto-calibrating analog value readers with full range.
static AnalogReader steps_reader(SPEED_POT_PIN, 1023);
static AnalogReader color_reader(COLOR_POT_PIN, 1023);

struct Settings {
  int steps_direction;
  int steps_speed;
  int color_direction;
  int color_speed;

  // Add the function as a static function here, to workaround
  // a bug where ino(?) creates an incorrect forward declaration.
  static Settings get_settings() {
    // Read the pot values.
    int movement = steps_reader.get_value();
    int color    = color_reader.get_value();

    Settings settings;

    // Convert the values into suitable directions and speed values.
    set_direction_and_speed(movement, settings.steps_direction, settings.steps_speed);
    set_direction_and_speed(color,    settings.color_direction, settings.color_speed);

    return settings;
  }
};

struct State {
  int start_led;
  int color_index;

public:
  void step_leds(Settings settings) {
    if (settings.steps_direction > 0) {
      start_led++;
      start_led %= NUM_LEDS;
    } else if (settings.steps_direction < 0) {
      start_led--;
      start_led += NUM_LEDS;
      start_led %= NUM_LEDS;
    } else {
      // Stand still.
    }
  }

  void step_color(Settings settings) {
    if (settings.color_direction > 0) {
      color_index++;
      color_index %= NUM_RAINBOW_LEDS;
    } else if (settings.color_direction < 0) {
      color_index--;
      color_index += NUM_RAINBOW_LEDS;
      color_index %= NUM_RAINBOW_LEDS;
    } else {
      // LEDs don't shift.
    }
  }
};

class CountTrigger {
  uint32_t _counter;
  uint32_t _level;
public:
  CountTrigger() : _counter(0), _level(0) {}
  void set_trigger_level(uint32_t level) {
    _level = level;
  }
  // Count up and return true iff the counter overflowed.
  bool tick() {
    _counter++;
    if (_counter >= _level) {
      _counter = 0;
      return true;
    }

    return false;
  }
};

static int reverse_and_expo(int value) {
  // I want the pots in the middle to mean no speed and fully turned pots to mean full speed,
  // I need to reverse the [0, 512] values.
  value = 512 - abs(value);
  if (value == 0) {
    value = 1;
  }

  // An exponential function with arbitrary parameters, that works well for my hardware. :)
  // It tries to make the center point stable, but still get a "fast" enough end points.
  return 1 + pow(2, value / 64.0);
}

// This function runs over and over, and is where you do the magic to light
// your leds.
void loop() {
  static State state;
  static CountTrigger steps_trigger;
  static CountTrigger color_trigger;

  // Read the settings from the pots.
  Settings settings = Settings::get_settings();

  // Turn off LEDs.
  for (int i = 0; i < NUM_RAINBOW_LEDS; i++) {
    leds[(state.start_led + i) % NUM_LEDS] = CRGB::Black;
  }

  // The timing is handled by naively counting the times in the loop,
  // and progressing the leds (or colors) if the counter overflowed.
  // Yes, this is very non-portable, but does the job.

  int steps_overflow_value = reverse_and_expo(settings.steps_speed);
  int color_overflow_value = reverse_and_expo(settings.color_speed);

  steps_trigger.set_trigger_level(steps_overflow_value);
  color_trigger.set_trigger_level(color_overflow_value);

  if (steps_trigger.tick()) {
    // The steps counter overflowed, time to move the LEDs.
    state.step_leds(settings);
  }
  if (color_trigger.tick()) {
    // The color counter overflowed, time to move the colors.
    state.step_color(settings);
  }

  // Set the LED colors.
  for (int i = 0; i < NUM_RAINBOW_LEDS; i++) {
    uint8_t led_pos   = (state.start_led   + i) % NUM_LEDS;
    uint8_t color_pos = (state.color_index + i) % NUM_RAINBOW_LEDS;
    leds[led_pos] = CRGB(red[color_pos], green[color_pos], blue[color_pos]);
  }

  FastLED.show();
}
