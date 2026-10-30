#pragma once

#include "mode.h"

namespace ca {

class Life : public Mode {
 public:
  static constexpr int kN = 80;
  static constexpr int kSpecies = 4;
  static constexpr uint8_t kTrail = 240;

  Life();
  const char* name() const override { return "Life"; }
  int variants() const override { return 3; }
  const char* variantName() const override;
  int variant() const override { return variant_; }
  void start(uint32_t seed, int variant) override;
  void tick() override;
  void renderRow(int y, uint16_t* out) const override;

  void clear();
  void set(int x, int y, int species);
  void step();
  bool alive(int x, int y) const { return cell_[y * kN + x] != 0; }
  int species(int x, int y) const { return cell_[y * kN + x] - 1; }
  uint8_t trail(int x, int y) const { return alive(x, y) ? 0 : age_[y * kN + x]; }
  int population() const;
  int activity() const { return births_ + deaths_; }

 private:
  void splash();

  uint8_t cell_[kN * kN];
  uint8_t next_[kN * kN];
  uint8_t age_[kN * kN];
  uint8_t hue_[kN * kN];
  uint16_t live_[kSpecies][8];
  uint16_t fade_[kSpecies][16];
  uint32_t birthMask_ = 0, surviveMask_ = 0;
  int births_ = 0, deaths_ = 0;
  int quiet_ = 0;
  uint32_t frame_ = 0;
  int variant_ = 0;
  Rng rng_;
};

class Brain : public Mode {
 public:
  static constexpr int kN = 120;
  enum State : uint8_t { Off = 0, On = 1, Dying = 2 };

  Brain();
  const char* name() const override { return "Brian's Brain"; }
  int variants() const override { return 2; }
  const char* variantName() const override;
  int variant() const override { return variant_; }
  void start(uint32_t seed, int variant) override;
  void tick() override;
  void renderRow(int y, uint16_t* out) const override;

  void clear();
  void set(int x, int y, State s) { cell_[y * kN + x] = s; }
  State at(int x, int y) const { return static_cast<State>(cell_[y * kN + x]); }
  void step();
  int population() const;

 private:
  uint8_t cell_[kN * kN];
  uint8_t next_[kN * kN];
  uint8_t glow_[kN * kN];
  uint16_t pal_[256];
  uint16_t on_ = 0, dying_ = 0;
  uint32_t frame_ = 0;
  int variant_ = 0;
  Rng rng_;
};

class Cyclic : public Mode {
 public:
  static constexpr int kN = 120;
  static constexpr int kMaxStates = 16;
  static constexpr int kMaxOffsets = 48;

  struct Rule {
    const char* name;
    uint8_t range;
    uint8_t threshold;
    uint8_t states;
    bool moore;
  };

  Cyclic();
  const char* name() const override { return "Cyclic"; }
  int variants() const override;
  const char* variantName() const override { return rule_.name; }
  int variant() const override { return variant_; }
  void start(uint32_t seed, int variant) override;
  void tick() override;
  void renderRow(int y, uint16_t* out) const override;

  void setRule(const Rule& r);
  void fill(uint8_t s);
  void set(int x, int y, uint8_t s) { cell_[y * kN + x] = s; }
  uint8_t at(int x, int y) const { return cell_[y * kN + x]; }
  int step();

 private:
  uint8_t cell_[kN * kN];
  uint8_t next_[kN * kN];
  int8_t dx_[kMaxOffsets], dy_[kMaxOffsets];
  int offsets_ = 0;
  Rule rule_{};
  uint16_t pal_[kMaxStates];
  int variant_ = 0;
  Rng rng_;
};

class Physarum : public Mode {
 public:
  static constexpr int kN = 240;
  static constexpr int kAgents = 6000;

  struct Params {
    const char* name;
    uint8_t sensorAngle;
    uint8_t sensorDist;
    uint8_t turn;
    uint8_t deposit;
    uint8_t decay;
    uint8_t spawn;
    uint8_t palette;
  };

  Physarum();
  ~Physarum() override;
  const char* name() const override { return "Slime Mold"; }
  int variants() const override;
  const char* variantName() const override { return p_.name; }
  int variant() const override { return variant_; }
  void start(uint32_t seed, int variant) override;
  void tick() override;
  void renderRow(int y, uint16_t* out) const override;

  void step();
  void diffuse();
  uint8_t trail(int x, int y) const { return trail_[y * kN + x]; }
  uint8_t* trail() { return trail_; }
  float agentX(int i) const { return x_[i]; }
  float agentY(int i) const { return y_[i]; }

 private:
  uint8_t sense(float x, float y, uint8_t heading) const;

  uint8_t* trail_ = nullptr;
  uint8_t* tmp_ = nullptr;
  float* x_ = nullptr;
  float* y_ = nullptr;
  uint8_t* heading_ = nullptr;
  float cos_[256], sin_[256];
  uint16_t pal_[256];
  Params p_{};
  int variant_ = 0;
  Rng rng_;
};

class GrayScott : public Mode {
 public:
  static constexpr int kN = 120;
  static constexpr int kOne = 1 << 14;

  struct Params {
    const char* name;
    float feed;
    float kill;
    uint8_t palette;
  };

  GrayScott();
  ~GrayScott() override;
  const char* name() const override { return "Reaction-Diffusion"; }
  int variants() const override;
  const char* variantName() const override { return p_.name; }
  int variant() const override { return variant_; }
  void start(uint32_t seed, int variant) override;
  void tick() override;
  void renderRow(int y, uint16_t* out) const override;

  void setParams(const Params& p);
  void fill(float u, float v);
  void drop(int cx, int cy, int r);
  int step();
  float u(int x, int y) const { return static_cast<float>(u_[y * kN + x]) / kOne; }
  float v(int x, int y) const { return static_cast<float>(v_[y * kN + x]) / kOne; }

 private:
  int16_t* u_ = nullptr;
  int16_t* v_ = nullptr;
  int16_t* u2_ = nullptr;
  int16_t* v2_ = nullptr;
  int32_t feed_ = 0, kill_ = 0;
  uint16_t pal_[256];
  Params p_{};
  int quiet_ = 0;
  int variant_ = 0;
  Rng rng_;
};

}  // namespace ca
