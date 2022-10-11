/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

import java.awt.FlowLayout;
import java.awt.Frame;
import java.awt.GraphicsConfiguration;
import java.awt.GraphicsDevice;
import java.awt.GraphicsEnvironment;
import java.awt.Insets;
import java.awt.Point;
import java.awt.PopupMenu;
import java.awt.Rectangle;
import java.awt.Robot;
import java.awt.Toolkit;
import java.awt.event.InputEvent;
import java.awt.event.MouseAdapter;
import java.awt.event.MouseEvent;
import java.io.File;
import java.util.concurrent.atomic.AtomicReference;
import java.util.stream.IntStream;
import javax.imageio.ImageIO;
import javax.swing.SwingUtilities;


/**
 * @test
 * @key headful
 * @bug 8160270
 * @run main/timeout=300 PopupMenuLocation
 */
public final class PopupMenuLocation {

    private static final int SIZE = 350;
    public static final String TEXT =
            "Long-long-long-long-long-long-long text in the item-";
    private static volatile boolean action = false;
    private static Robot robot;
    private static Frame frame;
    private static Rectangle currentScreenBounds;


    public static void main(final String[] args) throws Exception {
        robot = new Robot();
        robot.setAutoDelay(200);
        GraphicsEnvironment ge =
                GraphicsEnvironment.getLocalGraphicsEnvironment();
        GraphicsDevice[] sds = ge.getScreenDevices();
        for (GraphicsDevice sd : sds) {
            GraphicsConfiguration gc = sd.getDefaultConfiguration();
            currentScreenBounds = gc.getBounds();
            Point point = new Point(currentScreenBounds.x, currentScreenBounds.y);
            Insets insets = Toolkit.getDefaultToolkit().getScreenInsets(gc);
            while (point.y < currentScreenBounds.y + currentScreenBounds.height - insets.bottom - SIZE) {
                while (point.x <
                           currentScreenBounds.x + currentScreenBounds.width - insets.right - SIZE) {
                    test(point);
                    point.translate(currentScreenBounds.width / 5, 0);
                   }
                   point.setLocation(currentScreenBounds.x, point.y + currentScreenBounds.height / 5);
                }
            }
        }

    private static void test(final Point tmp) throws Exception {
        frame = new Frame();
        PopupMenu pm = new PopupMenu();
        IntStream.rangeClosed(1, 6).forEach(i -> pm.add(TEXT + i));
        pm.addActionListener(e -> {
            action = true;
            System.out.println(" Got action event " + e);
        });

        try {
            frame.setUndecorated(true);
            frame.setAlwaysOnTop(true);
            frame.setLayout(new FlowLayout());
            frame.add(pm);
            frame.pack();
            frame.setSize(SIZE, SIZE);
            frame.setVisible(true);
            frame.setLocation(tmp.x, tmp.y);

            frame.addMouseListener(new MouseAdapter() {
                public void mousePressed(MouseEvent e) {
                    show(e);
                }

                public void mouseReleased(MouseEvent e) {
                    show(e);
                }

                private void show(MouseEvent e) {
                    if (e.isPopupTrigger()) {
                        System.out.println("Going to show popup "+pm+" on "+frame);
                        pm.show(frame, 0, 50);
                    }
                }
            });
            openPopup(frame);
        } finally {
            frame.dispose();
        }
    }

    private static void openPopup(final Frame frame) throws Exception {
        robot.waitForIdle();
        Point pt = frame.getLocationOnScreen();
        int x = pt.x + frame.getWidth() / 2;
        int y = pt.y + 50;
        robot.mouseMove(x, y);
        robot.mousePress(InputEvent.BUTTON3_DOWN_MASK);
        robot.mouseRelease(InputEvent.BUTTON3_DOWN_MASK);
        robot.waitForIdle();
         y = y+50;
        robot.mouseMove(x, y);
        robot.mousePress(InputEvent.BUTTON1_DOWN_MASK);
        robot.mouseRelease(InputEvent.BUTTON1_DOWN_MASK);
        robot.waitForIdle();
        if (!action) {
            captureScreen();
            throw new RuntimeException(
                    "Failed, Not received the PopupMenu ActionEvent yet on " +
                    "frame= "+frame+", isFocused = "+frame.isFocused());
        }
        action = false;
    }

    private static void captureScreen() {
        try {
            ImageIO.write(robot.createScreenCapture(currentScreenBounds), "png",
                          new File("screen1.png"));
        } catch (Exception exception) {
            exception.printStackTrace();
        }
        action = false;
    }

}
