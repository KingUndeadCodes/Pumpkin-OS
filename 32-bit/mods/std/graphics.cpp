#include "../dev/serial/serial.h"
#include "./include/graphics.h"
#include "../dev/cmos/cmos.h"
#include "../dev/pit/pit.h"
#include "../dev/kb/kb.h"

#define reverse(b) (b * 0x0202020202ULL & 0x010884422010ULL) % 0x3ff

class View {
    public:
        int bg;
        short w;
        short h;
        short x;
        short y;
        View** child;

        /**
         * @brief Construct a new View object
         * @param w width
         * @param h height
         * @param x x position
         * @param y y position
         * @param bg background color (optional, default 0 <black>)
        */
        View(short w, short h, short x, short y, int bg = 0) {
            this->w = w;
            this->h = h;
            this->x = x;
            this->y = y;
            this->bg = bg;
        }

        ~View() {
            serial_write_string("View destructor called\n", false, NONE);
        }

        /**
         * @brief Draw the view
         * @param x x position
         * @param y y position
        */
        virtual void Draw(short x, short y) {
            if (this->bg != 0) { Screen::FromRangetoRange(this->x, this->x + this->w, this->y, this->y + this->h, this->bg); } 
            else if (this->x == 0 && this->y == 0 && this->w == 320 && this->h == 200) { Screen::DrawImage(); }
            for (View** child = this->child; (*child) != NULL; child++) {
                if ((*child)->w <= 0 && (*child)->h <= 0) { break; } // this line causes a hang.
                if ((*child)->bg != 0) {
                    int x1 = (*child)->x + this->x;
                    int x2 = x1 + (*child)->w;
                    int y1 = (*child)->y + this->y;
                    int y2 = y1 + (*child)->h;
                    int bg = (*child)->bg == 256 ? 0 : (*child)->bg; // 0 is transparent
                    Screen::FromRangetoRange(x1, x2, y1, y2, bg);
                }
                (*child)->Draw(this->x, this->y);
                continue;
            }
            return;
        }

        void *operator new(size_t size) { return malloc(size); }
        void *operator new[](size_t size) { return malloc(size); }
        void operator delete(void *p) { free(p); }
        void operator delete[](void *p) { free(p); }
};

class Character : public View {
    public:
        char c;

        /**
         * @brief Construct a new Character object
         * @param c character
         * @param x x position
         * @param y y position
        */
        Character(char c, short x, short y) : View(8, 8, x, y) {
            this->x = x;
            this->y = y;
            this->w = 8;
            this->h = 8;
            this->c = c;
            this->child = NULL;
        }

        void Draw(short x, short y) override {
            Screen::DrawChar(this->c, x + this->x, y + this->y);
        }

        static View string(const char* str, short x = 0, short y = 0) {
            int string_length = (int)strlen(str);
            View String(string_length * 8, ((int)(string_length / 40) + 1) * 8, 0, 0);
            View** childViews;
            String.x = x;
            String.y = y;
            int i = 0;
            for (; str[i] != '\0'; i++) childViews[i] = new Character((char)str[i], (short)(i * 8), 0);
            childViews[i] = NULL;
            String.child = childViews;
            return String;
        }
};

// Make a String View class based off Character assume std::string or any C++ standard library does not exsist
class String : public View {
    public:
        char* str;

        /**
         * @brief Construct a new String object
         * @param str string
         * @param x x position
         * @param y y position
        */
        String(char* str, short x, short y) : View((int)strlen(str) * 8, ((int)(strlen(str) / 40) + 1) * 8, x, y) {
            this->x = x;
            this->y = y;
            this->w = (int)strlen(str) * 8;
            this->h = ((int)(strlen(str) / 40) + 1) * 8;
            this->str = str;
        }

        void Draw(short x, short y) override {
            View String = Character::string(this->str, x + this->x, y + this->y);
            String.Draw(x, y);
        }
};

class Icon : public View {
    public:
        int e;

        /**
         * @brief Construct a new Icon object
         * @param e index into
         * @param x x position
         * @param y y position
        */
        Icon(int e, short x, short y) : View(32, 32, x, y) {
            this->e = e;
            this->x = x;
            this->y = y;
            this->w = 32;
            this->h = 32;
        }

        void Draw(short x, short y) override {
            Screen::DrawIcon(this->e, x + this->x, y + this->y);
        }
};

class Shape : public View {
    public: 
        Shape(short w, short h, short x, short y) : View(w, h, x, y) {
            this->x = x;
            this->y = y;
            this->w = w;
            this->h = h;
            this->child = NULL;
        }

        virtual void Scale(double factor) { 
            this->w = complexFloor(factor * this->w);
            this->h = complexFloor(factor * this->h);
            return;
        }

        // @future virtual void Rotate(double angle);
        // @future CustomShape Merge(Shape* shape);
};

class Rectangle : public Shape {
    public:
        Rectangle(short w, short h, short x, short y) : Shape(w, h, x, y) {
            this->x = x;
            this->y = y;
            this->w = w;
            this->h = h;
            this->child = NULL;
        }

        void Draw(short x, short y) override {
            for (short i = 0; i < w; i++) {
                for (short j = 0; j < h; j++) {
                    Screen::Plot(i + x + this->x, j + y + this->y, 0x5);
                }
            }
        }
};

class Circle : public Shape {
    public:
        int r;

        Circle(int r, short x, short y) : Shape(r * 2, r * 2, x, y) {
            this->r = r;
            this->x = x;
            this->y = y;
            this->w = r * 2;
            this->h = r * 2;
            this->child = NULL;
        }

        // Write a function to draw a cricle
        void Draw(short x, short y) override {
            for (short i = -r; i <= r; i++) {
                for (short j = -r; j <= r; j++) {
                    if (i * i + j * j <= r * r) {
                        Screen::Plot(i + this->x + x, j + this->y + y);
                    }
                }
            }
        }

        void Scale(double factor) override {
            this->r = complexFloor(factor * this->r); // This does not work with doubles for some reason.
            this->w = this->r * 2;
            this->h = this->r * 2;
        }
};

class CustomShape : public Shape {
    public:
        Shape* a;
        Shape* b;

        CustomShape(const Shape& a, const Shape& b) : Shape(calculateCompositeWidth(a, b), calculateCompositeHeight(a, b), min(a.x, b.x), min(a.y, b.y)) {
            this->x = min(a.x, b.x);
            this->y = min(a.y, b.y);
            this->w = calculateCompositeWidth(a, b);
            this->h = calculateCompositeHeight(a, b);
            this->a = (Shape*)&a;
            this->b = (Shape*)&b;
            return;
        }

        void Draw(short x, short y) override {
            this->b->Draw(x, y);
            this->a->Draw(x, y);
        }

        void Scale(double factor) override {
            this->a->Scale(factor);
            this->b->Scale(factor);
            this->w = calculateCompositeWidth(*this->a, *this->b);
            this->h = calculateCompositeHeight(*this->a, *this->b);
        }

    private:
        static short calculateCompositeWidth(const Shape& a, const Shape& b) {
            short rightMostEdge = max(a.x + a.w, b.x + b.w);
            short leftMostEdge = min(a.x, b.x);
            return rightMostEdge - leftMostEdge;
        }

        static short calculateCompositeHeight(const Shape& a, const Shape& b) {
            short bottomMostEdge = max(a.y + a.h, b.y + b.h);
            short topMostEdge = min(a.y, b.y);
            return bottomMostEdge - topMostEdge;
        }

        static short max(short a, short b) {
            return (a > b) ? a : b;
        }

        static short min(short a, short b) {
            return (a < b) ? a : b;
        }
};

class Calendar : public View {

    private:
        const int daysInMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        
        // Check if a given year is a leap year
        bool isLeapYear(int year) {
            return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        }

        // Zeller's Congruence algorithm to determine the day of the week
        int getFirstDayOfWeek(int year, int month) {
            if (month < 3) {
                month += 12;
                year -= 1;
            }
            int K = year % 100;
            int J = year / 100;
            int D = 1;
            int f = (D + (13 * (month + 1)) / 5 + K + K / 4 + J / 4 - 2 * J) % 7;
            int dayOfWeek = (f + 5) % 7;
            dayOfWeek = (dayOfWeek + 1) % 7;
            return dayOfWeek;
        }     

    public:

        enum class Month: int {
            January = 1,
            February = 2,
            March = 3,
            April = 4,
            May = 5,
            June = 6,
            July = 7,
            August = 8,
            September = 9,
            October = 10,
            November = 11,
            December = 12,
        };

        const char* calanderHeader(Month period) {
            switch ((int)period) {
                case 0x1: return "January";
                case 0x2: return "February";
                case 0x3: return "March";
                case 0x4: return "April";
                case 0x5: return "May";
                case 0x6: return "June";
                case 0x7: return "July";
                case 0x8: return "August";
                case 0x9: return "September";
                case 0xA: return "October";
                case 0xB: return "November";
                case 0xC: return "December";
                default: return "ERROR!";
            }
        }

        uint16_t year = 1970;
        Month month = Month::January;

        Calendar(short x, short y, uint16_t year, Month month) : View(168, 71, x, y) {
            this->year = year;
            this->month = month;
        }

        void Draw(short x, short y) {
            View myView = *this;
            View* childViews[7];      
            unsigned int indexTo = 0;
            char* header = (char*)malloc(25);
            strcat(header, "<- ");
            strcat(header, calanderHeader(month));
            strcat(header, " ");
            strcat(header, itoa(year, 10));
            strcat(header, " ->");
            childViews[indexTo++] = new String(header, 4, 4);
            childViews[indexTo++] = new String("S  M  T  W  T  F  S", 4, 14);
            // --------------------------------------------------------------
            char* buffer = (char*)malloc(100);
            int days = ((int)month == 2 && isLeapYear(year)) ? 29 : daysInMonth[(int)month - 1];
            int startDay = getFirstDayOfWeek(year, (int)month);
            char dayStr[4];
            for (int i = 0; i < startDay; ++i) buffer = strcat(buffer, "   ");
            for (int day = 1; day <= days; ++day) {
                strcpy(dayStr, itoa(day, 10));
                if (day < 10) buffer = strcat(buffer, "0");
                strcat(buffer, dayStr);
                strcat(buffer, " ");
                if ((day + startDay) % 7 == 0) {
                    char* str = (char*)malloc(strlen(buffer));
                    strcpy(str, buffer);
                    childViews[indexTo++] = new String(str, 4, 25 + ((indexTo - 2) * 9));
                    memset(buffer, 0, 100);
                    // TODO: free(str)
                }
            }
            if (strlen(buffer) > 0) {
                char* str = (char*)malloc(strlen(buffer));
                strcpy(str, buffer);
                childViews[indexTo++] = new String(str, 4, 25 + ((indexTo - 2) * 9));
                memset(buffer, 0, 100);
                // TODO: free(str)
            }
            myView.child = childViews; 
            myView.bg = 0x3;
            myView.Draw(x, y);
            free(header);
            free(buffer);
        }
};

char* CMOSTimeToString() {
    CMOSTime time = FetchCurrentCMOSTime();
    char* timeStr = (char*)malloc(8); // Format: "HH:MM:SS\0"
    // Convert hours, minutes, and seconds to strings and concatenate
    timeStr[0] = '0' + time.hours / 10;
    timeStr[1] = '0' + time.hours % 10;
    timeStr[2] = ':';
    timeStr[3] = '0' + time.minutes / 10;
    timeStr[4] = '0' + time.minutes % 10;
    timeStr[5] = ':';
    timeStr[6] = '0' + time.seconds / 10;
    timeStr[7] = '0' + time.seconds % 10;
    timeStr[8] = '\0'; // Null-terminate the string
    return timeStr;
}

namespace Modify {
    inline bool isChild(View* parent, View* child) {
        for (View** v = parent->child; (*v) != NULL; v++) {
            if ((*v) == child) return true;
        }
        return false;
    }
    View* alignment(View* v, Alignment to) {
        v->x = (to % 3 == 0) ? 0 : (to % 3 == 1) ? (320 - v->w) / 2 : 320 - v->w;
        v->y = (to <= 2) ? 0 : (to >= 3 && to <= 5) ? (200 - v->h) / 2 : 200 - v->h;
        return v;
    }
    View* alignment(View* parent, View* v, Alignment to) {
        if (!isChild(parent, v)) {
            serial_write_string("View Pointer `v` is not a child of View Pointer `parent`.\n", false, FAIL);
        } else {
            v->x = (to % 3 == 0) ? 0 : (to % 3 == 1) ? (parent->x + parent->w - v->w) / 2 : parent->x + parent->w - v->w;
            v->y = (to <= 2) ? 0 : (to >= 3 && to <= 5) ? (parent->y + parent->h - v->h) / 2 : parent->y + parent->h - v->h;
        }
        return v;
    }
}

void testable_function() {
    serial_write_string("Testable\n");
    return;
}

void setup_kb() {
    kb_add_event(-10, &testable_function);
    kb_add_event(-20, &testable_function);
    kb_add_event(-30, &testable_function);
    kb_add_event(-40, &testable_function);
}

namespace ViewTest {

    // ViewTest::SplashScreen();
    // ViewTest::CustomShapeTest();
    // ViewTest::CalendarTest();
    // ViewTest::HelloWorld();

    void HelloWorld(void) {
        View MainView(320, 200, 0, 0);
        View Application(304, 190, 0, 0);
        String String("Hello, World!", 0, 0);
        View* childViews_1[] = {&String, NULL};
        View* childViews_2[] = {&Application, NULL};
        Application.child = childViews_1;
        MainView.child = childViews_2;
        Modify::alignment(&Application, Alignment::center);
        Modify::alignment(&Application, &String, Alignment::center);
        MainView.Draw(0, 0);
        return;
    }

    void CalendarTest(void) {
        setup_kb();
        View MainView(320, 200, 0, 0);
        View Application(304, 190, 0, 0, 0x4);
        Calendar cal(0, 0, 2024, Calendar::Month::July);
        Icon x(0, 0, 0);
        View* childViews_1[] = {&cal, &x, NULL};
        View* childViews_2[] = {&Application, NULL};
        Application.child = childViews_1;
        MainView.child = childViews_2;
        Modify::alignment(&Application, Alignment::center);
        Modify::alignment(&Application, &cal, Alignment::trailing);
        Modify::alignment(&Application, &x, Alignment::leading);
        MainView.Draw(0, 0);
        return;
    }

    void CustomShapeTest(void) {
        Rectangle rect(60, 25, 130, 75);
        Circle arc = Circle(60, 160, 100);
        Rectangle box(50, 50, 0, 0);
        CustomShape shape = CustomShape(rect, arc);
        CustomShape shape2 = CustomShape(shape, box);
        shape2.Scale(0.5);
        shape2.Draw(0, 0);
    }

    void SplashScreen(void) {
        View MainView(320, 200, 0, 0);
        View MainSpan(320, 50, 0, 70, 0x4F);
        Icon Icon_1(0, 50, 9);
        Icon Icon_2(3, 172, 9);
        String String(CMOSTimeToString(), 100, 23); // "Welcome"
        Icon_1.bg = 0x4;
        Icon_2.bg = 0x4;
        String.bg = 0x4;
        View* childViews_1[] = {&Icon_1, &Icon_2, &String};
        View* childViews_2[] = {&MainSpan, NULL}; // If it's only one element, make sure to NULL terminate it.
        MainSpan.child = childViews_1;
        MainView.child = childViews_2;
        MainView.Draw(0, 0);
        while (true) {
            const char* str = CMOSTimeToString();
            memcpy(String.str, str, strlen(str));
            MainView.Draw(0, 0);
            timer_wait(18);
        };
        yield();
        timer_wait(18);
        serial_write_string("Finished\n", false, NONE);
        return;
    }

}

/// Set Pixel<x, y> to the color c
void DrawPixel(int x, int y, int c = 0xF) {
    unsigned char* Pixel = (unsigned char*)0xA0000 + 320 * y + x;
    *Pixel = c;
    return;
}

namespace Screen {
    static int offsetTextX = 0;
    static int offsetTextY = 0; 
    void Plot(int x, int y, int c = 0xF) {
        DrawPixel(x, y, c);
    }
    // Set the entire screen to the color c
    void Fill(int c = 0x3) {
        for (int x = 0; x < 320; x++) {
            for (int y = 0; y < 200; y++) {
                DrawPixel(x, y, c);
            }
        }
    }
    void FromRangetoRange(int fromX, int toX, int fromY, int toY, int color) {
        for (int i = fromY; i < toY; i++) {
            for (int j = fromX; j < toX; j++) { DrawPixel(j, i, color); }
        }
    }
    void DrawImage(void) {
        int x = 0;
        int y = 0;
        for (int i = 0; i < sizeof(Image) / sizeof(struct IntegerRange); i++) {
            for (int j = 0; j < Image[i].continueFor; j++) {
                if (x >= 320) {
                    x = 0;
                    y += 1;
                }
                DrawPixel(x++, y, (int)Image[i].continueWith);        
            }
        }
    }
    void DrawIcon(int e = 0, int x = 25, int y = 25) {
        // e = index into
        for (int i = 0; i < 32; i++) {
            for (int j = 0; j < 32; j++) {
                if (Icons[e][i][j] == 0x00) continue;
                if (Icons[e][i][j] == 0x10) DrawPixel(j + x, i + y, 0x0); 
                DrawPixel(j + x, i + y, Icons[e][i][j]); 
            }
        }
    }
    // Draws character x to the screen using the defined font.
    void DrawChar(char c, int x = NULL, int y = NULL) {
        if (offsetTextX == 320) {
            offsetTextY += 10;
            offsetTextX = 0;
        }
        if (c == ' ' || c == '\n') {
            if (c == '\n') {
                offsetTextY += 10; 
                offsetTextX = 0;
            }
            else if (c == ' ') offsetTextX += 8;
            return;
        }
        for (int i = 0; i < 8; i++) {
            for (int n = 0; n < 8; n++) {
                if ((Font[c][i] >> n) & 1) {
                    if (x != NULL && y != NULL) {
                        DrawPixel(n + x, i + y);
                    } else if (x != NULL) {
                        DrawPixel(n + x, i + offsetTextY);
                    } else if (y != NULL) {
                        DrawPixel(n + offsetTextX, i + y);
                    } else {
                        DrawPixel(n + offsetTextX, i + offsetTextY);
                    }
                }
            }
        }
        offsetTextX += 8;
    }
}