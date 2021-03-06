//////////////////////////////////////////////////////////////////////////////
// NoLifeClient - Part of the NoLifeStory project                           //
// Copyright © 2013 Peter Atashian                                          //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////

#include "sprite.hpp"
#include "config.hpp"
#include "time.hpp"
#include "view.hpp"
#include <nx/bitmap.hpp>
#include <GL/glew.h>
#include <unordered_map>
#include <deque>
#include <iostream>
#include <cmath>

namespace nl {
    double const pi = 3.14159265358979323846264338327950288419716939937510582;
    std::unordered_map<size_t, GLuint> textures;
    std::deque<size_t> loaded_textures;
    size_t last_bound = 0;
    GLuint vbo = 0;
    void sprite::init() {
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        float a[] = {0, 0, 1, 0, 1, 1, 0, 1};
        glBufferData(GL_ARRAY_BUFFER, sizeof(a), a, GL_STATIC_DRAW);
    }
    void sprite::reinit() {
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(2, GL_FLOAT, 0, nullptr);
        glTexCoordPointer(2, GL_FLOAT, 0, nullptr);
    }
    void sprite::bind() {
        bitmap b = current;
        if (b.id() == last_bound) return;
        if (!last_bound) reinit();
        last_bound = b.id();
        GLuint & t = textures[b.id()];
        if (!t) {
            glGenTextures(1, &t);
            glBindTexture(GL_TEXTURE_2D, t);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, b.width(), b.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, b.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            loaded_textures.push_back(b.id());
        } else {
            glBindTexture(GL_TEXTURE_2D, t);
        }
    }
    void sprite::unbind() {
        last_bound = 0;
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
        glLoadIdentity();
    }
    void sprite::cleanup() {
        while (static_cast<int>(loaded_textures.size()) > config::max_textures) {
            size_t s = loaded_textures.front();
            loaded_textures.pop_front();
            auto it = textures.find(s);
            glDeleteTextures(1, &it->second);
            textures.erase(it);
        }
    }
    sprite::sprite() {}
    sprite::sprite(node o) : data(o) {
        if (data.data_type() == node::type::bitmap) {
            animated = false;
        } else if (data["0"].data_type() == node::type::bitmap) {
            animated = true;
        } else {
            data = {};
        }
        set_frame(0);
    }
    void sprite::set_frame(int f) {
        if (!data) return;
        frame = f;
        if (!animated) {
            current = data;
        } else {
            current = data[frame];
            if (!current) {
                current = data[frame = 0];
            }
            delay = 0;
            next_delay = current["delay"].get_real(100);
        }
        bitmap b = current;
        width = b.width();
        height = b.height();
        node o = current["origin"];
        originx = o.x();
        originy = o.y();
        if (current["moveType"]) {
            movetype = current["moveType"];
            movew = current["moveW"];
            moveh = current["moveH"];
            movep = current["moveP"];
            mover = current["moveR"];
        }
        repeat = current["repeat"].get_bool();
        if (current["a0"] || current["a1"]) {
            a0 = current["a0"].get_real(0) / 255.;
            a1 = current["a1"].get_real(0) / 255.;
        } else {
            a0 = 1;
            a1 = 1;
        }
    }
    void sprite::draw(int x, int y, flags f, int cx, int cy) {
        if (!data) return;
        if (animated) {
            delay += time::delta * 1000;
            if (delay > next_delay) {
                set_frame(frame + 1);
            }
        }
        //cx and cy represent tiling distance
        if (!cx) cx = width;
        else if (cx < 0) cx = -cx;
        if (!cy) cy = height;
        else if (cy < 0) cy = -cy;
        //A bit of origin and view shifting
        f & flipped ? x -= width - originx : x -= originx;
        y -= originy;
        if (f & relative) {
            x -= view::xmin;
            y -= view::ymin;
        }
        //Handling movetypes
        double angle = 0;
        switch (movetype) {
        case 0:
            break;
        case 1:
            if (movep) x += static_cast<int>(movew * sin(time::delta_total * 1000 * 2 * pi / movep));
            else x += static_cast<int>(movew * sin(time::delta_total));
            break;
        case 2:
            if (movep) y += static_cast<int>(moveh * sin(time::delta_total * 1000 * 2 * pi / movep));
            else y += static_cast<int>(moveh * sin(time::delta_total));
            break;
        case 3:
            angle = time::delta_total * 1000 * 180 / pi / mover;
            break;
        default:
            std::cerr << "Unknown move type: " << movetype << std::endl;
        }
        int xbegin = x;
        int xend = x;
        int ybegin = y;
        int yend = y;
        if (f & tilex) {
            xbegin += width;
            xbegin %= cx;
            if (xbegin <= 0) xbegin += cx;
            xbegin -= width;
            xend -= view::width;
            xend %= cx;
            if (xend >= 0) xend -= cx;
            xend += view::width;
            if (xend < xbegin) return;
        }
        if (f & tiley) {
            ybegin += height;
            ybegin %= cy;
            if (ybegin <= 0) ybegin += cy;
            ybegin -= height;
            yend -= view::height;
            yend %= cy;
            if (yend >= 0) yend -= cy;
            yend += view::height;
            if (yend < ybegin) return;
        }
        if (xend + width < 0) return;
        if (xbegin > view::width) return;
        if (yend + height < 0) return;
        if (ybegin > view::height) return;
        if (config::rave) {
        } else if (animated) {
            double dif = delay / next_delay;
            glColor4d(1, 1, 1, dif * a1 + (1 - dif) * a0);
        } else {
            glColor4d(1, 1, 1, 1);
        }
        bind();
        if (f & tilex && cx == width) {
            if (f & tiley && cy == height) {
                xend += cx;
                yend += cy;
                int xm = (xend - xbegin) / cx;
                int ym = (yend - ybegin) / cy;
                glLoadIdentity();
                glBegin(GL_QUADS);
                glTexCoord2i(0, 0);
                glVertex2i(xbegin, ybegin);
                glTexCoord2i(xm, 0);
                glVertex2i(xend, ybegin);
                glTexCoord2i(xm, ym);
                glVertex2i(xend, yend);
                glTexCoord2i(0, ym);
                glVertex2i(xbegin, yend);
                glEnd();
            } else {
                xend += cx;
                int xm = (xend - xbegin) / cx;
                glLoadIdentity();
                glBegin(GL_QUADS);
                for (y = ybegin; y <= yend; y += cy) {
                    glTexCoord2i(0, 0);
                    glVertex2i(xbegin, y);
                    glTexCoord2i(xm, 0);
                    glVertex2i(xend, y);
                    glTexCoord2i(xm, 1);
                    glVertex2i(xend, y + height);
                    glTexCoord2i(0, 1);
                    glVertex2i(xbegin, y + height);
                }
                glEnd();
            }
        } else if (f & tiley && cy == height) {
            yend += cy;
            int ym = (yend - ybegin) / cy;
            glLoadIdentity();
            glBegin(GL_QUADS);
            for (x = xbegin; x <= xend; x += cx) {
                glTexCoord2i(0, 0);
                glVertex2i(x, ybegin);
                glTexCoord2i(1, 0);
                glVertex2i(x + width, ybegin);
                glTexCoord2i(1, ym);
                glVertex2i(x + width, yend);
                glTexCoord2i(0, ym);
                glVertex2i(x, yend);
            }
            glEnd();
        } else for (x = xbegin; x <= xend; x += cx) for (y = ybegin; y <= yend; y += cy) {
            glLoadIdentity();
            glTranslated(x + originx, y + originy, 0);
            glRotated(angle, 0, 0, 1);
            glTranslated(f & flipped ? width - originx : -originx, -originy, 0);
            glScaled(f & flipped ? -width : width, height, 1);
            glDrawArrays(GL_QUADS, 0, 4);
        }
        /*
        if (tilex) {
            if (tiley) {
                if (!cx) cx = w;
                int32_t x1 = x % cx - cx;
                int32_t x2 = (x - View::Width) % cx + View::Width + cx;
                if (!cy) cy = h;
                int32_t y1 = y % cy - cy;
                int32_t y2 = (y - View::Height) % cy + View::Height + cy;
                if (cx == w && cy == h) {
                    glBegin(GL_QUADS);
                    glTexCoord2i(0, 0);
                    glVertex2i(x1, y1);
                    glTexCoord2i((x2 - x1) / cx, 0);
                    glVertex2i(x2, y1);
                    glTexCoord2i((x2 - x1) / cx, (y2 - y1) / cy);
                    glVertex2i(x2, y2);
                    glTexCoord2i(0, (y2 - y1) / cy);
                    glVertex2i(x1, y2);
                    glEnd();
                } else for (x = x1; x < x2; x += cx) for (y = y1; y < y2; y += cy) single();
            } else {
                if (!cx) cx = w;
                int32_t x1 = x % cx - cx;
                int32_t x2 = (x - View::Width) % cx + View::Width + cx;
                for (x = x1; x < x2; x += cx) single();
            }
        } else {
            if (tiley) {
                if (!cy) cy = h;
                int32_t y1 = y % cy - cy;
                int32_t y2 = (y - View::Height) % cy + View::Height + cy;
                for (y = y1; y < y2; y += cy) single();
            } else if (x + w > 0 && x < View::Width && y + h > 0 && y < View::Height) single();
        }*/
    }
    sprite::flags & operator|=(sprite::flags & a, sprite::flags b) {
        a = static_cast<sprite::flags>(static_cast<int>(a) | static_cast<int>(b));
        return a;
    }
    sprite::flags operator|(sprite::flags a, sprite::flags b) {
        return static_cast<sprite::flags>(static_cast<int>(a) | static_cast<int>(b));
    }
}
