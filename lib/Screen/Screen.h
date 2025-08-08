

class Screen {
    protected:
        bool pauseNavigation = false;
        
    public:
        virtual ~Screen() = default;
        virtual void onEnter() = 0;
        virtual void onUpdate() = 0;
        virtual void onExit() = 0;
        
        // Navigation control methods
        bool isNavigationPaused() const { return pauseNavigation; }
        void setNavigationPaused(bool paused) { pauseNavigation = paused; }
};