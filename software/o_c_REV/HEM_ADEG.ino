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

class ADEG : public HemisphereApplet {
public:

    const char* applet_name() { // Maximum 10 characters
        return "AD EG";
    }

    void Start() {
        signal = 0;
        phase = 0;
        for(int i = 0; i < available_tabs; i++) {
            attack[i] = 50 * i;
            decay[i] = 50 - (i * 10);
        }
    }

    void Controller() {
        if (Clock(0)) {
            // Trigger the envelope
            phase = 1; // Return to attack phase
            effective_attack = attack[current_tab];
            effective_decay = decay[current_tab];
        } else if (Clock(1)) {
            // Trigger the envelope in reverse
            phase = 1;
            effective_attack = decay[current_tab];
            effective_decay = attack[current_tab];
        }

        if (phase > 0) {
            simfloat target;
            if (phase == 1) target = int2simfloat(HEMISPHERE_MAX_CV); // Rise to max for attack[current_tab]
            if (phase == 2) target = 0; // Fall to zero for decay[current_tab]

            if (signal != target) {
                int segment = phase == 1
                    ? effective_attack + Proportion(DetentedIn(0), HEMISPHERE_MAX_CV, HEM_ADEG_MAX_VALUE)
                    : effective_decay + Proportion(DetentedIn(1), HEMISPHERE_MAX_CV, HEM_ADEG_MAX_VALUE);
                segment = constrain(segment, 0, HEM_ADEG_MAX_VALUE);
                simfloat remaining = target - signal;

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
                signal += delta;

                if (simfloat2int(signal) >= HEMISPHERE_MAX_CV && phase == 1) phase = 2;

                // Check for EOC
                if (simfloat2int(signal) <= 0 && phase == 2) {
                    ClockOut(1);
                    phase = 0;
                }
            }
            Out(0, simfloat2int(signal));
        }
    }

    void View() {
        gfxHeader(applet_name());
        gfxTabs(available_tabs, current_tab);
        DrawIndicator();
    }

    void OnButtonPress() {
        // cursor = 1 - cursor;
        if (++current_tab == available_tabs) current_tab = 0;
    }

    void OnEncoderMove(int direction) {
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
    // Tabs
    int available_tabs = 4;
    int current_tab = 0;

    simfloat signal; // Current signal level for each channel
    int phase; // 0=Not running 1=Attack 2=Decay
    int cursor; // 0 = Attack, 1 = Decay
    int last_ms_value;
    int last_change_ticks;
    int effective_attack; // Attack and decay for this particular triggering
    int effective_decay;  // of the EG, so that it can be triggered in reverse!

    // Settings
    int *attack = new int(available_tabs); // Time to reach signal level if signal < 5V
    int *decay = new int(available_tabs); // Time to reach signal level if signal > 0V

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
        gfxRect(1, 15, ProportionCV(ViewOut(0), 62), 6);

        // Change indicator, if necessary
        if (OC::CORE::ticks - last_change_ticks < 20000) {
            gfxPrint(15, 43, last_ms_value);
            gfxPrint("ms");
        }
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
ADEG ADEG_instance[2];

void ADEG_Start(bool hemisphere) {ADEG_instance[hemisphere].BaseStart(hemisphere);}
void ADEG_Controller(bool hemisphere, bool forwarding) {ADEG_instance[hemisphere].BaseController(forwarding);}
void ADEG_View(bool hemisphere) {ADEG_instance[hemisphere].BaseView();}
void ADEG_OnButtonPress(bool hemisphere) {ADEG_instance[hemisphere].OnButtonPress();}
void ADEG_OnEncoderMove(bool hemisphere, int direction) {ADEG_instance[hemisphere].OnEncoderMove(direction);}
void ADEG_ToggleHelpScreen(bool hemisphere) {ADEG_instance[hemisphere].HelpScreen();}
uint32_t ADEG_OnDataRequest(bool hemisphere) {return ADEG_instance[hemisphere].OnDataRequest();}
void ADEG_OnDataReceive(bool hemisphere, uint32_t data) {ADEG_instance[hemisphere].OnDataReceive(data);}
