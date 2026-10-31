// GameController observation and snapshots. SEE: (CAKE.md, macOS input and controllers)
#include "cake_macos_private.h"
#if defined(__APPLE__)
#import <Cocoa/Cocoa.h>
#import <GameController/GameController.h>
static NSMutableArray<GCController*>* cake_macos_held_controllers = nil;
static id cake_macos_connect_observer = nil;
static id cake_macos_disconnect_observer = nil;
static NSTimer* cake_macos_reconcile_timer = nil;

static void cake_macos_report_controller(GCController* controller) {

    if (!controller.extendedGamepad) {
        return;
    }
    if ([cake_macos_held_controllers containsObject:controller]) {
        return;
    }

    const char* name = controller.vendorName.UTF8String;
    if (cake_macos_controller_connected((__bridge void*)controller, name ? name : "Gamepad")) {
        [cake_macos_held_controllers addObject:controller];
    }

    return;
}

static void cake_macos_reconcile_controllers(void) {
    NSArray<GCController*>* live = GCController.controllers;

    for (GCController* held in [cake_macos_held_controllers copy]) {
        if (![live containsObject:held]) {
            cake_macos_controller_disconnected((__bridge void*)held);
            [cake_macos_held_controllers removeObject:held];
        }
    }

    for (GCController* controller in live) {
        cake_macos_report_controller(controller);
    }

    return;
}

void cake_macos_start_controllers(void) {
    if (cake_macos_held_controllers) {
        return;
    }

    cake_macos_held_controllers = [NSMutableArray array];

    cake_macos_connect_observer =
        [[NSNotificationCenter defaultCenter] addObserverForName:GCControllerDidConnectNotification
                                                          object:nil
                                                           queue:nil
                                                      usingBlock:^(NSNotification* notification) {
                                                        cake_macos_report_controller(notification.object);
                                                      }];

    cake_macos_disconnect_observer = [[NSNotificationCenter defaultCenter]
        addObserverForName:GCControllerDidDisconnectNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* notification) {
                  GCController* controller = notification.object;
                  if ([cake_macos_held_controllers containsObject:controller]) {
                      cake_macos_controller_disconnected((__bridge void*)controller);
                      [cake_macos_held_controllers removeObject:controller];
                  }
                }];

    cake_macos_reconcile_timer = [NSTimer timerWithTimeInterval:2.0
                                                        repeats:YES
                                                          block:^(NSTimer* timer) {
                                                            (void)timer;
                                                            cake_macos_reconcile_controllers();
                                                          }];
    [[NSRunLoop mainRunLoop] addTimer:cake_macos_reconcile_timer forMode:NSRunLoopCommonModes];

    for (GCController* controller in GCController.controllers) {
        cake_macos_report_controller(controller);
    }

    return;
}

void cake_macos_stop_controllers(void) {
    if (cake_macos_reconcile_timer) {
        [cake_macos_reconcile_timer invalidate];
        cake_macos_reconcile_timer = nil;
    }
    if (cake_macos_connect_observer) {
        [[NSNotificationCenter defaultCenter] removeObserver:cake_macos_connect_observer];
        cake_macos_connect_observer = nil;
    }
    if (cake_macos_disconnect_observer) {
        [[NSNotificationCenter defaultCenter] removeObserver:cake_macos_disconnect_observer];
        cake_macos_disconnect_observer = nil;
    }
    cake_macos_held_controllers = nil;

    return;
}

bool cake_macos_read_controller(void* controller_object, CAKE_ControllerState* output) {
    if (!controller_object || !output) {
        return false;
    }
    GCController* controller = (__bridge GCController*)controller_object;
    GCExtendedGamepad* gamepad = controller.extendedGamepad;
    if (!gamepad) {
        return false;
    }

    uint16_t buttons = 0;
    if (gamepad.buttonA.pressed) {
        buttons |= CAKE_BUTTON_A;
    }
    if (gamepad.buttonB.pressed) {
        buttons |= CAKE_BUTTON_B;
    }
    if (gamepad.buttonX.pressed) {
        buttons |= CAKE_BUTTON_X;
    }
    if (gamepad.buttonY.pressed) {
        buttons |= CAKE_BUTTON_Y;
    }
    if (gamepad.leftShoulder.pressed) {
        buttons |= CAKE_BUTTON_LEFT_SHOULDER;
    }
    if (gamepad.rightShoulder.pressed) {
        buttons |= CAKE_BUTTON_RIGHT_SHOULDER;
    }
    if (gamepad.dpad.up.pressed) {
        buttons |= CAKE_BUTTON_DPAD_UP;
    }
    if (gamepad.dpad.down.pressed) {
        buttons |= CAKE_BUTTON_DPAD_DOWN;
    }
    if (gamepad.dpad.left.pressed) {
        buttons |= CAKE_BUTTON_DPAD_LEFT;
    }
    if (gamepad.dpad.right.pressed) {
        buttons |= CAKE_BUTTON_DPAD_RIGHT;
    }

    if (gamepad.buttonMenu.pressed) {
        buttons |= CAKE_BUTTON_START;
    }
    if (gamepad.buttonOptions.pressed) {
        buttons |= CAKE_BUTTON_BACK;
    }
    if (gamepad.leftThumbstickButton.pressed) {
        buttons |= CAKE_BUTTON_LEFT_THUMB;
    }
    if (gamepad.rightThumbstickButton.pressed) {
        buttons |= CAKE_BUTTON_RIGHT_THUMB;
    }

    output->buttons = buttons;
    output->leftTrigger = cake_macos_convert_trigger(gamepad.leftTrigger.value);
    output->rightTrigger = cake_macos_convert_trigger(gamepad.rightTrigger.value);
    output->thumbLeftX = cake_macos_convert_axis(gamepad.leftThumbstick.xAxis.value);
    output->thumbLeftY = cake_macos_convert_axis(gamepad.leftThumbstick.yAxis.value);
    output->thumbRightX = cake_macos_convert_axis(gamepad.rightThumbstick.xAxis.value);
    output->thumbRightY = cake_macos_convert_axis(gamepad.rightThumbstick.yAxis.value);

    return true;
}

#endif
