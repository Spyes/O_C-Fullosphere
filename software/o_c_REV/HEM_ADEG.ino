// Copyright (c) 2018, Jason Justian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#define HEM_ADEG_MAX_VALUE 255
#define HEM_ADEG_MAX_TICKS 33333
#define HEM_ADEG_AVAILABLE_TABS 4
#define HEM_ADEG_MAX_OPS 2
#define HEM_ADEG_OPT_DIRECTION 0
#define HEM_ADEG_OPT_MOD_IN 1
#define HEM_ADEG_SETTINGS_PAGES 2
#define HEM_ADEG_SETTINGS_PER_PAGE 2

class ADEG : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "AD EG";
    }

    void Start() {
        for(int i = 0; i < HEM_ADEG_AVAILABLE_TABS; i++) {
            signal[i] = 0;
            phase[i] = 0;
            attack[i] = 50 * i;
            decay[i] = 50 - (i * 10);
            operation[i] = 0;
            mod_dest[i] = 0;
            current_settings_page[i] = 0;
            opt_cursor[i] = 0;
        }
    }

    void Controller() {
        for(int i = 0; i < HEM_ADEG_AVAILABLE_TABS; i++) {
            if (FClock(i)) {
                phase[i] = 1;
                if (operation[i]) { // reverse
                    effective_attack[i] = decay[i];
                    effective_decay[i] = attack[i];
                } else {
                    effective_attack[i] = attack[i];
                    effective_decay[i] = decay[i];
                }
            }

            if (phase[i] > 0) {
                simfloat target = phase[i] == 1 ? int2simfloat(HEMISPHERE_MAX_CV) : 0;
                int attack_cv = 0, decay_cv = 0;
                if(mod_dest[i] == DEST_ATTACK || mod_dest[i] == DEST_BOTH) {
                    attack_cv = Proportion(DetentedIn(i), HEMISPHERE_MAX_CV, HEM_ADEG_MAX_VALUE);
                }
                if (mod_dest[i] == DEST_DECAY || mod_dest[i] == DEST_BOTH) {
                    decay_cv = Proportion(DetentedIn(i), HEMISPHERE_MAX_CV, HEM_ADEG_MAX_VALUE);
                }
                 int segment = phase[i] == 1
                    ? effective_attack[i] + attack_cv
                    : effective_decay[i] + decay_cv;
                segment = constrain(segment, 0, HEM_ADEG_MAX_VALUE);
                simfloat remaining = target - signal[i];

                // The number of ticks it would take to get from 0 to HEMISPHERE_MAX_CV
                int max_change = Proportion(segment, HEM_ADEG_MAX_VALUE, HEM_ADEG_MAX_TICKS);

                // The number of ticks it would take to move the remaining amount at max_change
                int ticks_to_remaining = Proportion(simfloat2int(remaining), HEMISPHERE_MAX_CV, max_change);
                if (ticks_to_remaining < 0) ticks_to_remaining = -ticks_to_remaining;

                simfloat delta;
                if (ticks_to_remaining <= 0) {
                    delta = remaining;
                } else {
                    delta = remaining / ticks_to_remaining;
                }
                signal[i] += delta;

                // we've reached the max on Attack, switch to Decay
                if (simfloat2int(signal[i]) >= HEMISPHERE_MAX_CV && phase[i] == 1) phase[i] = 2;

                // Check for EOC
                if (simfloat2int(signal[i]) <= 0 && phase[i] == 2) {
                    // ClockOut(i);
                    phase[i] = 0;
                }
                Out(i, simfloat2int(signal[i]));
            }
        }
    }

    void View() {
        gfxHeader(applet_name());
        gfxTabs(HEM_ADEG_AVAILABLE_TABS, current_tab);
        DrawIndicator();
        DrawSettings();
    }

    void OnButtonPress(int button) {
        if (button == LEFT_ENCODER) {
                cursor = 1 - cursor;
        } else {
            opt_cursor[current_tab] += 1;
            if (opt_cursor[current_tab] == HEM_ADEG_SETTINGS_PER_PAGE) {
                current_settings_page[current_tab] += 1;
                if (current_settings_page[current_tab] == HEM_ADEG_SETTINGS_PAGES) {
                    current_settings_page[current_tab] = 0;
                }
                opt_cursor[current_tab] = 0;
            }
        }
    }

    void OnTabSwitch() {}

    void OnEncoderMove(int encoder, int direction) {
        if (encoder == LEFT_ENCODER) {
            if (cursor == 0) {
                attack[current_tab] = constrain(attack[current_tab] += direction, 0, HEM_ADEG_MAX_VALUE);
                last_ms_value = Proportion(attack[current_tab], HEM_ADEG_MAX_VALUE, HEM_ADEG_MAX_TICKS) / 17;
            }
            else {
                decay[current_tab] = constrain(decay[current_tab] += direction, 0, HEM_ADEG_MAX_VALUE);
                last_ms_value = Proportion(decay[current_tab], HEM_ADEG_MAX_VALUE, HEM_ADEG_MAX_TICKS) / 17;
            }
            last_change_ticks = OC::CORE::ticks;
        }
        else {
            if (current_settings_page[current_tab] == 0) {
                if (opt_cursor[current_tab] == HEM_ADEG_OPT_DIRECTION) {
                    operation[current_tab] += direction;
                    if (operation[current_tab] == HEM_ADEG_MAX_OPS) operation[current_tab] = 0;
                    if (operation[current_tab] < 0) operation[current_tab] = HEM_ADEG_MAX_OPS - 1;
                } else {
                    mod_dest[current_tab] += direction;
                    if (mod_dest[current_tab] == MOD_DEST_LAST) mod_dest[current_tab] = 0;
                    if (mod_dest[current_tab] < 0) mod_dest[current_tab] = MOD_DEST_LAST - 1;
                }
            }
        }
    }
        
    uint32_t OnDataRequest() {
        uint32_t data = 0;
        Pack(data, PackLocation {0,8}, attack[current_tab]);
        Pack(data, PackLocation {8,8}, decay[current_tab]);
        return data;
    }

    void OnDataReceive(uint32_t data) {
        attack[current_tab] = Unpack(data, PackLocation {0,8});
        decay[current_tab] = Unpack(data, PackLocation {8,8});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Trg 2=Trg Revers";
        help[HEMISPHERE_HELP_CVS]      = "1=A mod 2=D mod";
        help[HEMISPHERE_HELP_OUTS]     = "A=Output B=EOC";
        help[HEMISPHERE_HELP_ENCODER]  = "Attack/Decay";
        //                               "------------------" <-- Size Guide
    }
    
private:
    enum MOD_DESTS {
        DEST_NONE,
        DEST_ATTACK,
        DEST_DECAY,
        DEST_BOTH,
        MOD_DEST_LAST
    };

    // Tabs
    int *settings_pages = new int(HEM_ADEG_SETTINGS_PAGES);
    int *current_settings_page = new int(HEM_ADEG_AVAILABLE_TABS);

    const char *op_names[HEM_ADEG_MAX_OPS] = {"Trigger", "Reverse"};
    int *operation = new int(HEM_ADEG_AVAILABLE_TABS);

    const char *mod_dest_names[MOD_DEST_LAST] = {"None", "Attack", "Decay", "Both"};
    int *mod_dest = new int(HEM_ADEG_AVAILABLE_TABS);

    int cursor; // 0 = Attack, 1 = Decay
    int *opt_cursor = new int(HEM_ADEG_AVAILABLE_TABS);
    int last_ms_value;
    int last_change_ticks;
    simfloat *signal = new simfloat(HEM_ADEG_AVAILABLE_TABS); // Current signal level for each channel
    int *phase = new int(HEM_ADEG_AVAILABLE_TABS); // 0=Not running 1=Attack 2=Decay
    int *effective_attack = new int(HEM_ADEG_AVAILABLE_TABS); // Attack and decay for this particular triggering
    int *effective_decay = new int(HEM_ADEG_AVAILABLE_TABS);  // of the EG, so that it can be triggered in reverse!

    // Settings
    int *attack = new int(HEM_ADEG_AVAILABLE_TABS); // Time to reach signal level if signal < 5V
    int *decay = new int(HEM_ADEG_AVAILABLE_TABS); // Time to reach signal level if signal > 0V

    void DrawIndicator() {
        int a_x = Proportion(attack[current_tab], HEM_ADEG_MAX_VALUE, 31);
        int d_x = a_x + Proportion(decay[current_tab], HEM_ADEG_MAX_VALUE, 31);

        if (d_x > 0) { // Stretch to use the whole viewport
            a_x = Proportion(62, d_x, a_x);
            d_x = Proportion(62, d_x, d_x);
        }

        gfxLine(0, 62, a_x, 33, cursor == 1);
        gfxLine(a_x, 33, d_x, 62, cursor == 0);

        // Output indicators
        gfxRect(1, 23, ProportionCV(ViewOut(current_tab), 62), 6);

        // Change indicator, if necessary
        if (OC::CORE::ticks - last_change_ticks < 20000) {
            gfxPrint(15, 43, last_ms_value);
            gfxPrint("ms");
        }
    }

    void DrawSettings() {
        switch (current_settings_page[current_tab]) {
            case 0:
                gfxPrint(69, 24, "DIRECTION");
                gfxPrint(69, 32, op_names[operation[current_tab]]);
                gfxPrint(69, 47, "CV IN");
                gfxPrint(69, 55, mod_dest_names[mod_dest[current_tab]]);
                break;
            case 1:
                gfxPrint(69, 24, "ATTACK");
                gfxPrint(69, 32, op_names[operation[current_tab]]);
                gfxPrint(69, 47, "DECAY");
                gfxPrint(69, 55, mod_dest_names[mod_dest[current_tab]]);
                break;
        }

        gfxCursor(69, 40 + (opt_cursor[current_tab] * 23), 58);
    }
};


////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to ADEG,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
ADEG ADEG_instance;

void ADEG_Start(bool hemisphere) {ADEG_instance.BaseStart(hemisphere, HEM_ADEG_AVAILABLE_TABS);}
void ADEG_Controller(bool hemisphere, bool forwarding) {ADEG_instance.BaseController(forwarding);}
void ADEG_View(bool hemisphere) {ADEG_instance.BaseView();}
void ADEG_OnButtonPress(bool button) {ADEG_instance.OnButtonPress(button);}
void ADEG_OnEncoderMove(bool encoder, int direction) {ADEG_instance.OnEncoderMove(encoder, direction);}
void ADEG_ToggleHelpScreen(bool hemisphere) {ADEG_instance.HelpScreen();}
uint32_t ADEG_OnDataRequest(bool hemisphere) {return ADEG_instance.OnDataRequest();}
void ADEG_OnDataReceive(bool hemisphere, uint32_t data) {ADEG_instance.OnDataReceive(data);}
void ADEG_OnRightButtonPress() {ADEG_instance.BaseRightButtonPress();}
