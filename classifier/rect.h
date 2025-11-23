class Rect {
    public:
        int x;
        int y;
        int width;
        int height;

        Rect(
            int x = 0,
            int y = 0,
            int w = 0,
            int h = 0
        ) :
        x(x),
        y(y),
        width(w),
        height(h) {}

        bool operator==(const Rect& other) const {
            return x == other.x &&
                y == other.y &&
                width == other.width &&
                height == other.height;
        }

        bool operator!=(const Rect& other) const {
            return !(*this == other);
        }
};
