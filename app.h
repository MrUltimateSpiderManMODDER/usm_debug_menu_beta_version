#pragma once

#include "variable.h"
#include "variables.h"

#include "mstring.h"

struct game;

struct app {
    struct internal {
        mString field_0;
        int field_10;
        int field_14;
        int field_18;
        int field_1C;
        int field_20;
        float field_24;
        int field_28;

        // 0x005B85D0
        internal()
            : field_0()
        {
            this->field_1C = 640;
            this->field_20 = 480;
            this->field_24 = 1.3333334;
            this->field_10 = 0;
            this->field_14 = -1;

            this->field_0 = "";

            this->field_18 = 0;
            this->field_28 = 0;
        }

        void sub_5B8670()
        {

            CDECL_CALL(0x005B8670, this);
        }

        void begin_screen_recording(const mString& a2, int a3)
        {
            if (this->field_18 != 2) {
                this->field_18 = 2;
                this->field_0 = a2;
                this->field_14 = 0;
                os_developer_options::instance()->set_int(mString { "CAMERA_CENTRIC_STREAMER" }, a3);
            }
        }
        void end_screen_recording()
        {
            this->field_18 = 0;
            os_developer_options::instance()->set_int(mString { "CAMERA_CENTRIC_STREAMER" }, 0);
        }

    };
    };





