/******************************************************************************\
* Copyright (C) 2012-2014 Leap Motion, Inc. All rights reserved.               *
* Leap Motion proprietary and confidential. Not for distribution.              *
* Use subject to the terms of the Leap Motion SDK Agreement available at       *
* https://developer.leapmotion.com/sdk_agreement, or another agreement         *
* between Leap Motion and you, your company or other organization.             *
\******************************************************************************/

#include <iostream>
#include <cstring>
#include "Leap.h"

#include <xcb/xcb.h>
//#include <xcb/xcb_icccm.h>
//#include <xcb/xproto.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xtest.h>
#include <xcb/xcb_util.h>

#define MOUSE_1 1
#define MOUSE_2 2
#define MOUSE_3 3
#define MOUSE_SCROLL_UP 4
#define MOUSE_SCROLL_DOWN 5
#define L_CONTROL 0x25
#define L_ALT 0x40
#define F1 0x43
#define F4 0x46
#define L_SHIFT 0x32
#define TAB 0x17
#define KEY_W 0x19
#define KEY_T 0x1c
#define UP 0x6f
#define DOWN 0x74
#define LEFT 0x71
#define RIGHT 0x72
#define BACKSPACE 0x16

const std::string fingerNames[] = {"Thumb", "Index", "Middle", "Ring", "Pinky"};
const std::string boneNames[] = {"Metacarpal", "Proximal", "Middle", "Distal"};
const std::string stateNames[] = {"STATE_INVALID", "STATE_START", "STATE_UPDATE", "STATE_END"};

enum states_t {RUN, EXIT};

void processFrame(Leap::Frame &frame, Leap::Frame &prev_frame, xcb_connection_t *x_connection, xcb_window_t *x_root_window);
xcb_screen_t *screen_of_display (xcb_connection_t *c, int screen);

int main(int argc, char** argv) {
  //Leapmotion definitions:
  Leap::Controller controller;
  Leap::Frame frame, prev_frame;
  Leap::GestureList gestures;
  int64_t last_frame_id = 0;

  //xcb definitions:
  xcb_connection_t *x_connection;
  xcb_screen_t     *x_screen;
  int              screen_num;
  xcb_window_t     x_root_window = {0};

  states_t program_state = RUN;

  //wait until the controller is connected
  while (controller.isConnected());
  std::cout << "Leapmotion Connected\n";

  //init the leapmotion
  controller.enableGesture(Leap::Gesture::TYPE_CIRCLE);
  controller.enableGesture(Leap::Gesture::TYPE_KEY_TAP);
  controller.enableGesture(Leap::Gesture::TYPE_SCREEN_TAP);
  controller.enableGesture(Leap::Gesture::TYPE_SWIPE);

  if (argc > 1 && strcmp(argv[1], "--bg") == 0)
    controller.setPolicy(Leap::Controller::POLICY_BACKGROUND_FRAMES);

  //init the connection to the x server
  x_connection = xcb_connect(NULL, &screen_num);
  x_screen = screen_of_display(x_connection, screen_num);
  if (x_screen)
    x_root_window = x_screen->root;

  //verify that xtest exists
  xcb_test_get_version_cookie_t cookie = xcb_test_get_version(x_connection, 2, 1);

  xcb_generic_error_t *x_error = NULL;

  xcb_test_get_version_reply_t *xtest_reply =
    xcb_test_get_version_reply ( x_connection, cookie, &x_error );
  if (xtest_reply) {
    fprintf( stderr, "XTest version %u.%u\n",
      (unsigned int)xtest_reply->major_version,
      (unsigned int)xtest_reply->minor_version );
    free(xtest_reply);
  }

  if (x_error) {
    fprintf( stderr, "XTest version error: %d", (int)x_error->error_code );
    free(x_error);
    program_state = EXIT;
  }

  while (program_state == RUN) {
    frame = controller.frame(); //get the current frame
    prev_frame = controller.frame(1);

    if (last_frame_id != frame.id()) //only process frame if it updated
      processFrame(frame, prev_frame, x_connection, &x_root_window);

    last_frame_id = frame.id();
  } //end while

  xcb_disconnect(x_connection);

  return 0;
}

void processFrame(Leap::Frame &frame, Leap::Frame &prev_frame,
    xcb_connection_t *x_connection, xcb_window_t *x_root_window) {
  xcb_window_t none = { XCB_NONE };

  static int mouse_button_pressed = false;

  //handle mouse position and clicking.
  Leap::HandList hands = frame.hands();
  Leap::Hand right_hand = hands[0];
  if (right_hand.isValid()) {
    //do not move or click if fist
    if (right_hand.grabStrength() != 1) {
      xcb_warp_pointer(x_connection, XCB_NONE, *x_root_window, 0, 0, 0, 0,
          (right_hand.palmPosition().x+150)*(1600/(float)300),
          1200 - (1200/(float)175)*(right_hand.palmPosition().y-75));

      //hardcoded values must change!!!
      //std::cout << right_hand.pinchStrength() << std::endl;
      //
      if (right_hand.pinchStrength() >= 0.97) {
        xcb_test_fake_input(x_connection, XCB_BUTTON_PRESS,
            MOUSE_1, 0, none, 0, 0, 0);
        mouse_button_pressed = true;
      }
      else {
        if(mouse_button_pressed) {
          xcb_test_fake_input(x_connection, XCB_BUTTON_RELEASE,
              MOUSE_1, 0, none, 0, 0, 0);
          mouse_button_pressed = false;
        }
      }

      xcb_flush(x_connection);
    } //end if (right_hand.grabStrength
  } //end if(right_hand.isValid

  //std::cout << right_hand.palmPosition() << ", ";
  //std::cout << (right_hand.palmPosition().x+150)* (1600/(float)300)<< ", " <<
      //1200 - (1200/(float)175)*(right_hand.palmPosition().y-75) << std::endl;

  // Get gestures
  Leap::GestureList gestures = frame.gestures();
  for (int g = 0; g < gestures.count(); ++g) {
    Leap::Gesture gesture = gestures[g];

    switch (gesture.type()) {
      case Leap::Gesture::TYPE_CIRCLE:
        {
          Leap::CircleGesture circle = gesture;
          Leap::CircleGesture prev_circle =
            Leap::CircleGesture(prev_frame.gesture(circle.id()));
          std::string clockwiseness;

          if (circle.pointable().direction().angleTo(circle.normal()) <= Leap::PI/2) {
            clockwiseness = "clockwise";
            if ((floor(circle.progress()) - floor(prev_circle.progress())) == 1) {
              for (int i = 0; i < floor(floor(circle.radius())/5); i++){ //different size different speed
                xcb_test_fake_input(x_connection, XCB_BUTTON_PRESS,
                    MOUSE_SCROLL_DOWN, 0, none, 0, 0, 0);
                xcb_test_fake_input(x_connection, XCB_BUTTON_RELEASE,
                    MOUSE_SCROLL_DOWN, 0, none, 0, 0, 0);
              }
              xcb_flush(x_connection);
            }
          } //end if (circle.pointable
          else {
            clockwiseness = "counterclockwise";
            if ((floor(circle.progress()) - floor(prev_circle.progress())) == 1) {
              for (int i = 0; i < floor((floor(circle.radius()) - 10)/5); i++) {
                xcb_test_fake_input(x_connection, XCB_BUTTON_PRESS,
                    MOUSE_SCROLL_UP, 0, none, 0, 0, 0);
                xcb_test_fake_input(x_connection, XCB_BUTTON_RELEASE,
                    MOUSE_SCROLL_UP, 0, none, 0, 0, 0);
              }
              xcb_flush(x_connection);
            }
          } //end else

          // Calculate angle swept since last frame
          /*
          float sweptAngle = 0;
          if (circle.state() != Leap::Gesture::STATE_START) {
            Leap::CircleGesture previousUpdate = Leap::CircleGesture(controller.frame(1).gesture(circle.id()));
            sweptAngle = (circle.progress() - previousUpdate.progress()) * 2 * Leap::PI;
          }
          */

          std::cout << std::string(2, ' ')
            << "Circle id: " << gesture.id()
            << ", state: " << stateNames[gesture.state()]
            << ", progress: " << circle.progress()
            << ", radius: " << circle.radius()
            << ", distance: " << circle.radius()
            <<  ", " << clockwiseness << std::endl;
            break;
        }
      case Leap::Gesture::TYPE_SWIPE:
        {
          Leap::SwipeGesture swipe = gesture;
          Leap::Finger finger = Leap::Finger(swipe.pointable()); //grab finger

          if (gesture.state() == 1) { //if the gesture is starting
            if (finger.type() == 1) { //index finger
              //if it is in the y direction
              if (fabs(swipe.direction().y) > fabs(swipe.direction().x)) {
                xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_CONTROL, 0, none, 0, 0, 0);
                if (swipe.direction().y < 0) {
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, KEY_W, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, KEY_W, 0, none, 0, 0, 0);
                }
                else {
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_SHIFT, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, KEY_T, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, KEY_T, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_SHIFT, 0, none, 0, 0, 0);
                }
                xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_CONTROL, 0, none, 0, 0, 0);
              } //end if fabs

              else {
                xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_CONTROL, 0, none, 0, 0, 0);
                if (swipe.direction().x < 0) { //moving left
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_SHIFT, 0, none, 0, 0, 0);
                }
                xcb_test_fake_input(x_connection, XCB_KEY_PRESS, TAB, 0, none, 0, 0, 0);

                if (swipe.direction().x < 0) {
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_SHIFT, 0, none, 0, 0, 0);
                }

                xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, TAB, 0, none, 0, 0, 0);
                xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_CONTROL, 0, none, 0, 0, 0);
              }

            } //end if (finger type

            else if (finger.type() == 2) { //middle finger
              //if it is in the y direction
              if (fabs(swipe.direction().y) > fabs(swipe.direction().x)) {
                xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_CONTROL, 0, none, 0, 0, 0);
                xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_ALT, 0, none, 0, 0, 0);
                if (swipe.direction().y > 0) {
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, DOWN, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, DOWN, 0, none, 0, 0, 0);
                }
                else {
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, UP, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, UP, 0, none, 0, 0, 0);
                }

                xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_ALT, 0, none, 0, 0, 0);
                xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_CONTROL, 0, none, 0, 0, 0);

              } //end if(fabs
              else {
                if (swipe.direction().x < 0) {
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_ALT, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, LEFT, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, LEFT, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_ALT, 0, none, 0, 0, 0);
                }
                else {
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_ALT, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, RIGHT, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, RIGHT, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_ALT, 0, none, 0, 0, 0);
                }
              }
            } //end if (finger.type
            else if (finger.type() == 4) { //pinky finger
              if (fabs(swipe.direction().y) > fabs(swipe.direction().x)) {
                if (swipe.direction().y < 0) {
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_ALT, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, F4, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, F4, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_ALT, 0, none, 0, 0, 0);
                }
                else {
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, L_ALT, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_PRESS, F1, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, F1, 0, none, 0, 0, 0);
                  xcb_test_fake_input(x_connection, XCB_KEY_RELEASE, L_ALT, 0, none, 0, 0, 0);
                }
              } //end if (fabs
            } //end else if
          } //end if (gesture.state

          xcb_flush(x_connection);
          std::cout << std::string(2, ' ')
            << "Swipe id: " << gesture.id()
            << ", state: " << stateNames[gesture.state()]
            << ", direction: " << swipe.direction()
            << ", speed: " << swipe.speed() << std::endl;
            break;
        }
      case Leap::Gesture::TYPE_KEY_TAP:
        {
          Leap::KeyTapGesture tap = gesture;
          std::cout << std::string(2, ' ')
            << "Key Tap id: " << gesture.id()
            << ", state: " << stateNames[gesture.state()]
            << ", position: " << tap.position()
            << ", direction: " << tap.direction()<< std::endl;
            break;
        }
      case Leap::Gesture::TYPE_SCREEN_TAP:
        {
          Leap::ScreenTapGesture screentap = gesture;
          std::cout << std::string(2, ' ')
            << "Screen Tap id: " << gesture.id()
            << ", state: " << stateNames[gesture.state()]
            << ", position: " << screentap.position()
            << ", direction: " << screentap.direction()<< std::endl;
            break;
        }
      default:
        std::cout << std::string(2, ' ')  << "Unknown gesture type." << std::endl;
        break;
    } //end switch
  } //end for
}

xcb_screen_t *screen_of_display (xcb_connection_t *c, int screen) {
  xcb_screen_iterator_t iter;

  iter = xcb_setup_roots_iterator (xcb_get_setup (c));
  for (; iter.rem; --screen, xcb_screen_next (&iter))
    if (screen == 0)
      return iter.data;

  return NULL;
}
