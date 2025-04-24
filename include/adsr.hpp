#ifndef KEYBOARD_ADSR_HPP
#define KEYBOARD_ADSR_HPP
#include <ncurses.h>
#include <stdio.h>
#include <string>

class ADSR {
public:
  ADSR() {
    float duration = 0.8f;
    int samplerate = 44100;
    short amplitude = 32767;
    this->amplitude = amplitude;
    this->qadsr[0] = 1;
    this->qadsr[1] = 1;
    this->qadsr[2] = 3;
    this->qadsr[3] = 3;
    this->quantas = 8;
    this->quantas_length = static_cast<int>(samplerate * duration);
    this->length = this->quantas_length * this->quantas;
    this->sustain_level = 0.8 * amplitude;
    this->quantas = 1 + 1 + 3 + 3;
  }
  ADSR(short amplitude, int quantas_a, int quantas_d, int quantas_s,
       int quantas_r, float sustain_level, int quantas_length) {
    this->amplitude = amplitude;
    this->quantas = quantas_a + quantas_d + quantas_s + quantas_r;
    this->quantas_length = quantas_length;
    this->length = this->quantas_length * this->quantas;
    this->sustain_level = (short)(sustain_level * amplitude);

    this->qadsr[0] = quantas_a;
    this->qadsr[1] = quantas_d;
    this->qadsr[2] = quantas_s;
    this->qadsr[3] = quantas_r;
  }
  ADSR &operator=(const ADSR &other) {
    if (this != &other) {
      this->amplitude = other.amplitude;
      this->quantas =
          other.qadsr[0] + other.qadsr[1] + other.qadsr[2] + other.qadsr[3];
      this->length = other.length;
      this->quantas_length = other.quantas_length;
      this->sustain_level = other.sustain_level;

      this->qadsr[0] = other.qadsr[0];
      this->qadsr[1] = other.qadsr[1];
      this->qadsr[2] = other.qadsr[2];
      this->qadsr[3] = other.qadsr[3];
    }
    return *this;
  }

public:
  int response(int x) {
    int value = 0;
    int i;
    int q_acc = 0;
    int attack_end = this->quantas_length * this->qadsr[0];
    int decay_end = attack_end + this->quantas_length * this->qadsr[1];
    int sustain_end = decay_end + this->quantas_length * this->qadsr[2];
    int release_end = sustain_end + this->quantas_length * this->qadsr[3];
    if (x < attack_end)
      return attack(x);
    if (x < decay_end)
      return decay(x);
    if (x < sustain_end)
      return sustain();
    return release(x);
  }

  bool reached_sustain(int index) {
    int attack_end = this->quantas_length * this->qadsr[0];
    int decay_end = attack_end + this->quantas_length * this->qadsr[1];
    int sustain_end = decay_end + this->quantas_length * this->qadsr[2];
    int release_end = sustain_end + this->quantas_length * this->qadsr[3];
    if (index > decay_end) {
      return true;
    }
    return false;
  }

  int getLength() { return this->length; };
  void setLength(int length) {
    this->length = length;
    this->update_len();
  }

  short attack(int x) {
    return (short)this->amplitude *
           (((float)x) / (this->qadsr[0] * this->quantas_length));
  }
  short decay(int x) {
    int attack_end = this->quantas_length * this->qadsr[0];
    int decay_length = this->quantas_length * (this->qadsr[1]);
    return (short)(this->amplitude -
                   (((float)x - attack_end) / decay_length) *
                       (this->amplitude - this->sustain_level));
  }
  short sustain() { return (short)this->sustain_level; }
  short release(int x) {
    int sustain_end = this->quantas_length *
                      (this->qadsr[0] + this->qadsr[1] + this->qadsr[2]);
    int release_length = this->length - sustain_end;
    return x > getLength() ? 0
                           : (short)this->sustain_level -
                                 (((float)x - sustain_end) / release_length) *
                                     this->sustain_level;
  }

  void update_len() {
    this->quantas =
        this->qadsr[0] + this->qadsr[1] + this->qadsr[2] + this->qadsr[3];
    this->length = this->quantas_length * this->quantas;
  }

  int get_sustain_start_index() {
    int attack_end = this->quantas_length * this->qadsr[0];
    int decay_end = attack_end + this->quantas_length * this->qadsr[1];
    return decay_end;
  }

  std::string getCoolASCIVisualization(const std::string &prefix);

  short amplitude;
  int quantas;
  int qadsr[4];
  int length;
  int quantas_length;
  short sustain_level;
};

#endif
