#include "HSUtils.h"
#include "HemisphereAudioApplet.h"

template <AudioChannels Channels>
class FilterFolderApplet : public HemisphereAudioApplet {

public:
  enum FiltFoldCursor {
    FOLD_AMT,
    FOLD_CV,
    FILTMODE,
    FILTER_FREQ,
    FILTER_FREQ_CV,
    FILTER_RES,
    FILTER_RES_CV,
    AMP,
    AMP_CV,

    CURSOR_MAX = AMP_CV
  };

  const char* applet_name() {
    return "Filt/Fold";
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, filtfolder[i].folder, 0);
      out_conns[i].connect(filtfolder[i].mixer, 0, output, i);
    }
  }

  void Controller() {
    for (int i = 0; i < Channels; i++) {
      filtfolder[i].filter.frequency(PitchToRatio(pitch + pitch_cv.In()) * C3);
      filtfolder[i].filter.resonance(0.01f * (res + res_cv.InRescaled(500)));
      filtfolder[i].AmpAndFold(
        0.01f * fold * fold_cv.InF(1.0f),
        dbToScalar(amplevel) * amp_cv.InF(1.0f)
      );
    }
  }

  void View() {
    const char * const modename[] = {
      "", "+LPF", "+BPF", "+HPF"
    };
    const int label_x = 1;
    int label_y = 15;

    gfxPrint(label_x, label_y, "Fld: ");
    gfxStartCursor();
    graphics.printf("%3d%%", fold);
    gfxEndCursor(cursor == FOLD_AMT);
    gfxStartCursor();
    gfxPrintIcon(fold_cv.Icon());
    gfxEndCursor(cursor == FOLD_CV, false, fold_cv.InputName());

    label_y += 10;
    gfxIcon(label_x, label_y, PhzIcons::filter);
    gfxStartCursor(label_x + 9, label_y);
    gfxPrint("FOLD");
    gfxPrint(modename[filtfolder[0].modesel]);
    gfxEndCursor(cursor == FILTMODE);

    if (filtfolder[0].modesel) {
      label_y += 10;
      gfxStartCursor(label_x, label_y);
      gfxPrintPitchHz(pitch);
      gfxEndCursor(cursor == FILTER_FREQ);
      gfxStartCursor();
      gfxPrintIcon(pitch_cv.Icon());
      gfxEndCursor(cursor == FILTER_FREQ_CV, false, pitch_cv.InputName());

      label_y += 10;
      gfxPrint(label_x, label_y, "Res: ");
      gfxStartCursor();
      graphics.printf("%3d%%", res);
      gfxEndCursor(cursor == FILTER_RES);
      gfxStartCursor();
      gfxPrintIcon(res_cv.Icon());
      gfxEndCursor(cursor == FILTER_RES_CV, false, res_cv.InputName());
    }

    label_y += 10;
    gfxPrint(label_x, label_y, "Amp:");
    gfxStartCursor();
    gfxPrintDb(amplevel);
    gfxEndCursor(cursor == AMP);
    gfxStartCursor();
    gfxPrintIcon(amp_cv.Icon());
    gfxEndCursor(cursor == AMP_CV, false, amp_cv.InputName());

    gfxDisplayInputMapEditor();
  }

  void OnButtonPress() override {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(FILTER_FREQ_CV, pitch_cv),
          IndexedInput(FILTER_RES_CV, res_cv),
          IndexedInput(FOLD_CV, fold_cv),
          IndexedInput(AMP_CV, amp_cv)
        ))
      return;
    CursorToggle();
  }
  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      do {
        MoveCursor(cursor, direction, CURSOR_MAX);
      } while (0 == filtfolder[0].modesel && cursor >= FILTER_FREQ && cursor <= FILTER_RES_CV);
      return;
    }
    if (EditSelectedInputMap(direction)) return;
    switch (cursor) {
      case FILTMODE:
        ChangeMode(direction);
        break;
      case FILTER_FREQ:
        pitch = constrain(pitch + direction * 16, -8 * 12 * 128, 8 * 12 * 128);
        break;
      case FILTER_FREQ_CV:
        pitch_cv.ChangeSource(direction);
        break;
      case FILTER_RES:
        res = constrain(res + direction, 70, 500);
        break;
      case FILTER_RES_CV:
        res_cv.ChangeSource(direction);
        break;
      case FOLD_AMT:
        fold = constrain(fold + direction, 0, 400);
        break;
      case FOLD_CV:
        fold_cv.ChangeSource(direction);
        break;
      case AMP:
        amplevel = constrain(amplevel + direction, LVL_MIN_DB, LVL_MAX_DB);
        break;
      case AMP_CV:
        amp_cv.ChangeSource(direction);
        break;
    }
  }

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    data[0] = PackPackables(pitch, res, fold, amplevel, filtfolder[0].modesel);
    data[1] = PackPackables(pitch_cv, res_cv, fold_cv, amp_cv);
  }

  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    UnpackPackables(data[0], pitch, res, fold, amplevel, filtfolder[0].modesel);
    UnpackPackables(data[1], pitch_cv, res_cv, fold_cv, amp_cv);
    if (Channels == STEREO)
      filtfolder[1].modesel = filtfolder[0].modesel;
  }

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

protected:
  void SetHelp() override {}

private:
  int cursor = 0;
  int16_t pitch = 1 * 12 * 128; // C4
  CVInputMap pitch_cv;
  int16_t res = 75;
  CVInputMap res_cv;
  int16_t fold = 6; // 0% is mute, 6% is dry
  CVInputMap fold_cv;
  int8_t amplevel = 0;
  CVInputMap amp_cv;

  struct FilterFolder {
    AudioEffectWaveFolder folder;
    AudioFilterStateVariable filter;
    AudioSynthWaveformDc drive;
    AudioMixer4 mixer;

    uint8_t modesel = 0; // 0 = BYPASS, 1 = LPF, 2 = BPF, 3 = HPF

    AudioConnection conn0{folder, 0, filter, 0};
    AudioConnection conn2{folder, 0, mixer, 0};
    AudioConnection conn1{filter, 0, mixer, 1};
    AudioConnection conn1a{filter, 1, mixer, 2};
    AudioConnection conn1b{filter, 2, mixer, 3};

    AudioConnection conn4{drive, 0, folder, 1};

    void ChangeMode(int8_t dir = 1) {
      modesel = constrain(modesel + dir, 0, 3);
    }
    void AmpAndFold(float foldF, float level) {
      drive.amplitude(foldF);
      for (int i = 0; i < 4; ++i) {
        mixer.gain(i, (i == modesel) * level);
      }
    }
  };

  AudioPassthrough<Channels> input;
  std::array<FilterFolder, Channels> filtfolder;
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> out_conns;

  void ChangeMode(int dir) {
    uint8_t newmode = constrain(filtfolder[0].modesel + dir, 0, 3);
    for (int i = 0; i < Channels; i++) {
      filtfolder[i].modesel = newmode;
    }
  }
};
