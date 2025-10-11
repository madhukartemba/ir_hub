class Fader {
   private:
    float value = 0.0f;   // current fade position (0 → 1)
    float speed = 1.0f;   // speed factor (1 = normal, >1 = faster)
    bool active = false;  // whether a fade is currently in progress

   public:
    Fader() = default;

    void start(float initialValue = 0.0f, float speed_ = 1.0f) {
        value = initialValue;
        speed = speed_;
        active = true;
    }
    void update(float deltaTime) {
        if (!active) return;

        value += speed * deltaTime;  // increment based on elapsed time
        if (value >= 1.0f) {
            value = 1.0f;
            active = false;  // fade complete
        }
    }
    float getValue() const { return value; }
    bool isComplete() const { return !active; }
    void setSpeed(float newSpeed) { speed = newSpeed; }
};
