#include "ksRuntimeController.h"
#include "ksRuntimeBoundary.h"

namespace ks {
using namespace controllerDetail;
// UART queues, timer/clock interrupts, hardware control and host transport.
// Fixed native control flow for the verified KS revision. Private ROM data
// remains external; instruction fetch reads account for original bus clocks.
bool RuntimeController::io(ControllerCpu& h){
    if(PanelMatrixCalls::candidate(unsigned(h.getPC())))return shared.panelMatrixCalls.step(h);
    if(PedalSenseCalls::candidate(unsigned(h.getPC())))return shared.pedalSenseCalls.step(h);
    if(PanelEncoderCalls::candidate(unsigned(h.getPC())))return shared.panelEncoderCalls.step(h);
    if(PanelAdcSamplesCalls::candidate(unsigned(h.getPC())))return shared.panelAdcSamplesCalls.step(h);
    if(PanelAdcScaleCalls::candidate(unsigned(h.getPC())))return shared.panelAdcScaleCalls.step(h);
    if(PanelAdcCalibrationCalls::candidate(unsigned(h.getPC())))return shared.panelAdcCalibrationCalls.step(h);
    if(PanelAdcIrqCalls::candidate(unsigned(h.getPC())))return shared.panelAdcIrqCalls.step(h);
    if(PanelAdcScanCalls::candidate(unsigned(h.getPC())))return shared.panelAdcScanCalls.step(h);
    if(InternalMidiQueueCalls::candidate(unsigned(h.getPC())))return shared.internalMidiQueueCalls.step(h);
    if(MidiOutputCalls::candidate(unsigned(h.getPC())))return shared.midiOutputCalls.step(h);
    if(InternalMidiServiceCalls::candidate(unsigned(h.getPC())))return shared.internalMidiServiceCalls.step(h);
    if(PanelAnalogCalls::candidate(unsigned(h.getPC())))return shared.panelAnalogCalls.step(h);
    if(ClockSyncCalls::candidate(unsigned(h.getPC())))return shared.clockSyncCalls.step(h);
    if(ClockPulseCalls::candidate(unsigned(h.getPC())))return shared.clockPulseCalls.step(h);
    if(ClockDisplayCalls::candidate(unsigned(h.getPC())))return shared.clockDisplayCalls.step(h);
    if(PanelSettingsCalls::candidate(unsigned(h.getPC())))return shared.panelSettingsCalls.step(h);
    if(SystemServicesCalls::candidate(unsigned(h.getPC())))return shared.systemServicesCalls.step(h);
    if(KeyboardEventsCalls::candidate(unsigned(h.getPC())))return shared.keyboardEventsCalls.step(h);
    if(KeyboardScanCalls::candidate(unsigned(h.getPC())))return shared.keyboardScanCalls.step(h);
    if(DiagnosticCodecCalls::candidate(unsigned(h.getPC())))return shared.diagnosticCodecCalls.step(h);
    if(DiagnosticTransferCalls::candidate(unsigned(h.getPC())))return shared.diagnosticTransferCalls.step(h);
    if(MemoryDiagnosticsCalls::candidate(unsigned(h.getPC())))return shared.memoryDiagnosticsCalls.step(h);
    if(StartupBannerCalls::candidate(unsigned(h.getPC())))return shared.startupBannerCalls.step(h);
    if(LoaderInputsCalls::candidate(unsigned(h.getPC())))return shared.loaderInputsCalls.step(h);
    if(LoaderCodecCalls::candidate(unsigned(h.getPC())))return shared.loaderCodecCalls.step(h);
    if(LoaderPacketsCalls::candidate(unsigned(h.getPC())))return shared.loaderPacketsCalls.step(h);
    if(LoaderProgramCalls::candidate(unsigned(h.getPC())))return shared.loaderProgramCalls.step(h);
    if(LoaderSessionCalls::candidate(unsigned(h.getPC())))return shared.loaderSessionCalls.step(h);
    if(PanelInputCalls::candidate(unsigned(h.getPC())))return shared.panelInputCalls.step(h);
    if(TimerServiceCalls::candidate(unsigned(h.getPC())))return shared.timerServiceCalls.step(h);
    if(FlashPageCalls::candidate(unsigned(h.getPC())))return shared.flashPageCalls.step(h);
    if(ExceptionReturnCalls::candidate(unsigned(h.getPC()))&&shared.exceptionReturnCalls.step(h))return true;
    if(DspFaultCalls::candidate(unsigned(h.getPC())))return shared.faultCalls.step(h);
    if(DspBootStreamCalls::candidate(unsigned(h.getPC())))return shared.bootStreamCalls.step(h);
    if(DspBootSetupCalls::candidate(unsigned(h.getPC())))return shared.bootSetupCalls.step(h);
    if(DspStartupCalls::candidate(unsigned(h.getPC())))return shared.startupCalls.step(h);
    if(HardwareSetupCalls::candidate(unsigned(h.getPC()))&&shared.hardwareSetupCalls.step(h))return true;
    if(PanelDecayCalls::candidate(unsigned(h.getPC()))&&shared.panelDecayCalls.step(h))return true;
    if(PanelByteCalls::candidate(unsigned(h.getPC()))&&shared.panelByteCalls.step(h))return true;
    if(PanelPollCalls::candidate(unsigned(h.getPC()))&&shared.panelPollCalls.step(h))return true;
    if(PanelActionCalls::candidate(unsigned(h.getPC()))&&shared.panelActionCalls.step(h))return true;
    if(SerialQueueCalls::candidate(unsigned(h.getPC()))&&shared.serialQueueCalls.step(h))return true;
    if(SerialReceiveCalls::candidate(unsigned(h.getPC()))&&shared.serialReceiveCalls.step(h))return true;
    if(SerialSetupCalls::candidate(unsigned(h.getPC()))&&shared.serialSetupCalls.step(h))return true;
    if(SerialTransmitCalls::candidate(unsigned(h.getPC()))&&shared.serialTransmitCalls.step(h))return true;
    if(SerialIrqCalls::candidate(unsigned(h.getPC()))&&shared.serialIrqCalls.step(h))return true;
    if(SerialDirectCalls::candidate(unsigned(h.getPC()))&&shared.serialDirectCalls.step(h))return true;
    if(PanelControlCalls::candidate(unsigned(h.getPC()))&&shared.panelControlCalls.step(h))return true;
    if(PanelToggleCalls::candidate(unsigned(h.getPC()))&&shared.panelToggleCalls.step(h))return true;
    if(PanelDecimalCalls::candidate(unsigned(h.getPC()))&&shared.panelDecimalCalls.step(h))return true;
    if(PanelTextCalls::candidate(unsigned(h.getPC()))&&shared.panelTextCalls.step(h))return true;
    if(PanelFormatCalls::candidate(unsigned(h.getPC()))&&shared.panelFormatCalls.step(h))return true;
    if(PanelHexCalls::candidate(unsigned(h.getPC()))&&shared.panelHexCalls.step(h))return true;
    if(PanelPercentCalls::candidate(unsigned(h.getPC()))&&shared.panelPercentCalls.step(h))return true;
    if(PanelVersionCalls::candidate(unsigned(h.getPC()))&&shared.panelVersionCalls.step(h))return true;
    if(PanelPublishCalls::candidate(unsigned(h.getPC()))&&shared.panelPublishCalls.step(h))return true;
    if(PanelMasksCalls::candidate(unsigned(h.getPC()))&&shared.panelMasksCalls.step(h))return true;
    if(PanelInitializeCalls::candidate(unsigned(h.getPC()))&&shared.panelInitializeCalls.step(h))return true;
    if(PanelDetectCalls::candidate(unsigned(h.getPC()))&&shared.panelDetectCalls.step(h))return true;
    if(FirmwareLayoutCheckCalls::candidate(unsigned(h.getPC()))&&shared.firmwareLayoutCheckCalls.step(h))return true;
    if(TimingTablesCalls::candidate(unsigned(h.getPC()))&&shared.timingTablesCalls.step(h))return true;
    return shared.voiceActions.step(h);
}
}
