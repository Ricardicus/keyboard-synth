#ifndef KEYBOARD_ADSR_HPP
#define KEYBOARD_ADSR_HPP
#include <stdio.h>
#include <ncurses.h>

class ADSR {
public:
  ADSR(short amplitude, int quantas_a, int quantas_d, int quantas_s,
       int quantas_r, float sustain_level, int length) {
    this->amplitude = amplitude;
    this->quantas = quantas_a + quantas_d + quantas_s + quantas_r;
    this->length = length;
    this->quantas_length = length / this->quantas;
    this->sustain_level = (short)(sustain_level * amplitude);

    this->qadsr[0] = quantas_a;
    this->qadsr[1] = quantas_d;
    this->qadsr[2] = quantas_s;
    this->qadsr[3] = quantas_r;
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
    if ( x < attack_end )
      return attack(x);
    if ( x < decay_end )
      return decay(x);
    if ( x < sustain_end )
      return sustain(x);
    return release(x);
  }

  int getLength() { return this->length; };

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
  short sustain(int x) {
    (void)x;
    return (short)this->sustain_level;
  }
  short release(int x) {
    int sustain_end = this->quantas_length *
                      (this->qadsr[0] + this->qadsr[1] + this->qadsr[2]);
    int release_length = this->length - sustain_end;
    return (short)this->sustain_level -
           (((float)x - sustain_end) / release_length) * this->sustain_level;
  }

  short amplitude;
  int quantas;
  int qadsr[4];
  int length;
  int quantas_length;
  short sustain_level;
};

#endif
