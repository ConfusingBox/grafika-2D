#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <thread>
#include <chrono>

#include "../Header/Util.h"
#include "../Sprite.h"

#include <algorithm> // za stdmin i stdmax

    

// KONSTANTE I GLOBALNE
const float ROPE_HALF_W = 0.001f;  // pola sirine konopca



const double TARGET_FPS = 75.0;
const double TARGET_FRAME_TIME = 1.0 / TARGET_FPS;

const float BOX_LEFT = -0.4f;
const float BOX_RIGHT = 0.4f;
const float BOX_BOTTOM = -0.5f;
const float BOX_TOP = 0.4f;

const float HOLE_LEFT = 0.15f;
const float HOLE_RIGHT = 0.35f;
const float HOLE_BOTTOM = -0.5f;
const float HOLE_TOP = -0.38f;

const float PRIZE_BOTTOM = -0.9f;
const float PRIZE_TOP = -0.5f;

// kandza
const float CLAW_SPEED = 0.6f;  // horizontalno
const float ROPE_SPEED = 0.6f;  // spustanje/podizanje
const float CLAW_HALF_H = 0.09f; // pola visine sprite-a (height 0.18f)

// gravitacija za pad igracaka
const float GRAVITY = -1.5f;   // smer ka dole (koordinate idu na minus)

bool gameActive = false;

bool prizeAvailable = false;


double lampTimer = 0.0;
bool  lampGreenPhase = false;  // false je crveno a true  zeleno


// STANJA

enum class ClawState { Idle, Lowering, Raising };

enum class ToyState {
    InBox,     // na dnu staklene kutije
    Grabbed,   // kandza drzi igracku
    Falling,   // pada (ili kroz rupu ili na dno)
    InPrize    // dole u prostoru za nagradu
};


// STRUKTURE

struct Toy {
    float x;
    float y;
    float halfSize;
    ToyState state;
    float vy; // brzina kod padanja
};


// GLOBALNE PROMENLJIVE

Sprite clawOpen;
Sprite clawClosed;

Sprite toyPig;   // crveni svinja
Sprite toyCow;   // plavi krava

Sprite jukeboxSprite;

Sprite cursorToken;

Sprite studentLabel;

Sprite background;


    


bool  clawIsOpen = true;
float clawX = 0.0f;
float clawY = BOX_TOP - CLAW_HALF_H;
float ropeLength = 0.0f;
ClawState clawState = ClawState::Idle;

bool clawHasToy = false;
int  grabbedToyIndex = -1;

// dve igracke
Toy toy1{ -0.15f, -0.45f, 0.05f, ToyState::InBox, 0.0f };
Toy toy2{ 0.05f, -0.45f, 0.05f, ToyState::InBox, 0.0f };
Toy* toys[2] = { &toy1, &toy2 };

bool sWasDown = false;   // da znamo da li je S bio pritisnut u prethodnom frejmu



// MAIN

int main()
{
    if (!glfwInit()) {
        return endProgram("GLFW init failed!");
    }

    // OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Primarni monitor + rezolucija
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (!primaryMonitor) {
        glfwTerminate();
        return endProgram("Nije pronadjen primarni monitor!");
    }

    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    if (!mode) {
        glfwTerminate();
        return endProgram("Nije pronadjen video mod monitora!");
    }

    int screenWidth = mode->width;
    int screenHeight = mode->height;

    // FULLSCREEN prozor
    GLFWwindow* window = glfwCreateWindow(
        screenWidth, screenHeight,
        "Kandza projekat",
        primaryMonitor,
        nullptr
    );
    if (!window) {
        glfwTerminate();
        return endProgram("GLFW window creation failed!");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // nas frame limiter

    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return endProgram("GLEW init failed!");
    }

    glViewport(0, 0, screenWidth, screenHeight);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //  Shaderi 
    unsigned int rectShader = createShader("Shaders/rect.vert", "Shaders/rect.frag");
    if (!rectShader) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return endProgram("Greska pri kreiranju sejdera za pravougaonik!");
    }

    unsigned int spriteShader = createShader("Shaders/sprite.vert", "Shaders/sprite.frag");
    if (!spriteShader) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return endProgram("Greska pri kreiranju sejdera za sprite!");
    }

    // sampler = texture unit 0
    glUseProgram(spriteShader);
    GLint texLoc = glGetUniformLocation(spriteShader, "uTexture");
    glUniform1i(texLoc, 0);
    glUseProgram(0);

    //  VAO/VBO za pravougaonike 
    int colorLoc = glGetUniformLocation(rectShader, "uColor");

    unsigned int rectVAO, rectVBO;
    glGenVertexArrays(1, &rectVAO);
    glGenBuffers(1, &rectVBO);

    glBindVertexArray(rectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    auto setRectVertices = [&](float left, float right, float bottom, float top)
        {
            float verts[] = {
                left,  bottom,
                right, bottom,
                right, top,
                right, top,
                left,  top,
                left,  bottom
            };

            glBindBuffer(GL_ARRAY_BUFFER, rectVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        };

    // UCITAVANJE TEKSTURA
    //if (!clawOpen.load("Textures/claw_open.png")) {
    //    std::cout << "Problem sa ucitavanjem claw_open.png\n";
    //}
    //if (!clawClosed.load("Textures/claw_closed.png")) {
    //    std::cout << "Problem sa ucitavanjem claw_closed.png\n";
    //}
    if (!clawOpen.load("Textures/piston.png")) {
        std::cout << "Problem sa ucitavanjem claw_open.png\n";
    }
    if (!clawClosed.load("Textures/spiston.png")) {
        std::cout << "Problem sa ucitavanjem claw_closed.png\n";
    }

    toyPig.load("Textures/pig.png");
    toyCow.load("Textures/cow.jpg");

    jukeboxSprite.load("Textures/jukebox.png");

    cursorToken.load("Textures/zeton.png");

    studentLabel.load("Textures/leon_label.png");

    background.loadRepeated("Textures/pozadina.jpg");
    background.uvRepeatX = 4.0f;
    background.uvRepeatY = 4.0f;



    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);




    //
    // 
    // GLAVNA PETLJA
    // 
    //

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // KLIK NA SLOT ZA ZETON 
        if (!gameActive && !prizeAvailable) { // samo ako igra nije vec aktivna i nagrada nije dostupna
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);

                // ekran -> NDC (-1..1)
                float ndcX = static_cast<float>((mx / screenWidth) * 2.0 - 1.0);
                float ndcY = static_cast<float>(1.0 - (my / screenHeight) * 2.0);

                // koordinate slota (ISTE kao u crtanju)
                float slotLeft = -0.10f;
                float slotRight = 0.10f;
                float slotBottom = -0.80f;
                float slotTop = -0.73f;

                bool inSlot =
                    (ndcX >= slotLeft && ndcX <= slotRight &&
                        ndcY >= slotBottom && ndcY <= slotTop);

                if (inSlot) {
                    gameActive = true;
                    // po zadatku: kandza se otvara kad igra krene
                    clawIsOpen = true;
                }
            }
        }

        //  KLIK NA IGRACKU U PRIZE PROSTORU 
        if (prizeAvailable &&
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            // ekran -> NDC (-1..1)
            float ndcX = static_cast<float>((mx / screenWidth) * 2.0 - 1.0);
            float ndcY = static_cast<float>(1.0 - (my / screenHeight) * 2.0);

            for (int i = 0; i < 2; ++i) {
                Toy& t = *toys[i];
                if (t.state != ToyState::InPrize) continue;

                float left = t.x - t.halfSize;
                float right = t.x + t.halfSize;
                float bottom = t.y - t.halfSize;
                float top = t.y + t.halfSize;

                bool inside =
                    (ndcX >= left && ndcX <= right &&
                        ndcY >= bottom && ndcY <= top);

                if (inside) {
                    // "pokupili" smo nagradu onda reset igracke nazad u kutiju
                    t.vy = 0.0f;
                    t.state = ToyState::InBox;

                    // vracamo ih na pocetne pozicije (isto kao u globalnoj inicijalizaciji)
                    if (&t == &toy1) {
                        t.x = -0.15f;
                        t.y = -0.45f;
                    }
                    else {
                        t.x = 0.05f;
                        t.y = -0.45f;
                    }

                    prizeAvailable = false;  // lampica se gasi
                    // gameActive ostaje false, igrac mora opet da klikne na slot
                    break;
                }
            }
        }




        // INPUT

       //  KRETANJE KANDZE (A / D / S) 
        if (gameActive) {

            bool sDown = (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);

            // A D S reaguju SAMO kad je kandza u Idle
            if (clawState == ClawState::Idle) {

                // A i D – pomeranje levo/desno
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                    clawX -= CLAW_SPEED * static_cast<float>(dt);
                }
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                    clawX += CLAW_SPEED * static_cast<float>(dt);
                }

                // clamp unutar kutije
                clawX = std::max(BOX_LEFT + 0.05f, std::min(BOX_RIGHT - 0.05f, clawX));

                //  S (edge detection) 
                if (sDown && !sWasDown) {
                    if (clawHasToy && grabbedToyIndex >= 0) {
                        // pustamo igracku da pada
                        Toy& t = *toys[grabbedToyIndex];
                        clawHasToy = false;
                        grabbedToyIndex = -1;
                        t.state = ToyState::Falling;
                        t.vy = 0.0f;
                        clawIsOpen = true;

                    }
                    else {
                        // krecemo spustanje
                        clawState = ClawState::Lowering;
                        clawIsOpen = true;
                    }
                }
            }

            // i dalje pratimo stanje S da edge detection radi
            sWasDown = sDown;
        }
        else {
            // ako igra nije aktivna, resetujem edge detection
            sWasDown = false;
        }


        // 
        // LOGIKA KANDZE
        // 

        float clawBottom = (clawY - ropeLength) - CLAW_HALF_H;

        switch (clawState)
        {
        case ClawState::Lowering:
        {
            clawIsOpen = true;
            ropeLength += ROPE_SPEED * static_cast<float>(dt);
            clawBottom = (clawY - ropeLength) - CLAW_HALF_H;

            // Pokusaj hvatanja igracke (samo ako jos nema)
            if (!clawHasToy) {
                for (int i = 0; i < 2; ++i) {
                    Toy& t = *toys[i];
                    if (t.state != ToyState::InBox) continue;

                    float toyLeft = t.x - t.halfSize;
                    float toyRight = t.x + t.halfSize;
                    float toyBottom = t.y - t.halfSize;
                    float toyTop = t.y + t.halfSize;

                    bool insideX = (clawX >= toyLeft && clawX <= toyRight);
                    bool insideY = (clawBottom <= toyTop && clawBottom >= toyBottom);

                    if (insideX && insideY) {
                        // UGRABILI SMO IGRACKU JUPIIIII
                        clawHasToy = true;
                        grabbedToyIndex = i;
                        t.state = ToyState::Grabbed;
                        clawState = ClawState::Raising;
                        clawIsOpen = false;
                        break;
                    }
                }
            }

            // Ako dodjem do dna praznih ruku dizem se gore
            if (clawBottom <= BOX_BOTTOM) {
                clawState = ClawState::Raising;
                if (!clawHasToy) {
                    clawIsOpen = false; // zatvara se iako je prazna
                }
            }
        }
        break;

        case ClawState::Raising:
        {
            ropeLength -= ROPE_SPEED * static_cast<float>(dt);
            if (ropeLength <= 0.0f) {
                ropeLength = 0.0f;
                clawState = ClawState::Idle;
            }
        }
        break;

        case ClawState::Idle:
        default:
            break;
        }

        // Ako kandza drzi igracku - prilepi je ispod kandze
        if (clawHasToy && grabbedToyIndex >= 0) {
            Toy& t = *toys[grabbedToyIndex];
            t.x = clawX;
            t.y = (clawY - ropeLength) - CLAW_HALF_H - t.halfSize;
        }

        // 
        // LOGIKA PADANJA IGRACAKA
        // 
        //
        //
        for (int i = 0; i < 2; ++i) {
            Toy& t = *toys[i];

            if (t.state == ToyState::Falling) {
                // gravitacija
                t.vy += GRAVITY * static_cast<float>(dt);
                t.y += t.vy * static_cast<float>(dt);

                float toyLeft = t.x - t.halfSize;
                float toyRight = t.x + t.halfSize;
                float toyBottom = t.y - t.halfSize;

                // stigla do nivoa dna kutije?
                if (toyBottom <= BOX_BOTTOM) {

                    // da li je CELA igracka iznad otvora rupe (horizontalno)?
                    bool overHoleHorizontally =
                        (toyLeft >= HOLE_LEFT) &&
                        (toyRight <= HOLE_RIGHT);

                    if (overHoleHorizontally) {
                        // igracka pada kroz otvor rupe ka prize prostoru
                        if (toyBottom <= PRIZE_BOTTOM) {
                            // stigla skroz dole u prize prostor
                            t.y = PRIZE_BOTTOM + t.halfSize;
                            t.vy = 0.0f;
                            t.state = ToyState::InPrize;

                            // osvojena nagrada
                            prizeAvailable = true;
                            gameActive = false;
                        }
                        // else: jos uvek je izmedju dna kutije i dna prize prostora – nastavlja da pada
                    }
                    else {
                        // nije potpuno iznad rupe onda udarila u dno kutije
                        t.y = BOX_BOTTOM + t.halfSize;
                        t.vy = 0.0f;
                        t.state = ToyState::InBox;
                    }
                }
            }
        }


        //  UPDATE LAMPICE 
        if (prizeAvailable) {
            lampTimer += dt;
            if (lampTimer >= 0.5) {
                lampTimer -= 0.5;
                lampGreenPhase = !lampGreenPhase;  // flip za zeleno i crveno
            }
        }
        else {
            lampTimer = 0.0;
            lampGreenPhase = false;
        }



        // RENDER


        glClear(GL_COLOR_BUFFER_BIT);

        background.x = 0.0f;
        background.y = 0.0f;
        background.width = 2.0f;  // ceo ekran u NDC
        background.height = 2.0f;

        background.draw(spriteShader);

        glUseProgram(rectShader);
        glBindVertexArray(rectVAO);

       /* glUseProgram(spriteShader);*/ //NE TREBA TU PRAVI PROBLEM



        // 1) Staklena kutija
        {
            setRectVertices(BOX_LEFT, BOX_RIGHT, BOX_BOTTOM, BOX_TOP);
            glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 0.2f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // 2) Rupa
        {
            setRectVertices(HOLE_LEFT, HOLE_RIGHT, HOLE_BOTTOM, HOLE_TOP);
            glUniform4f(colorLoc, 0.05f, 0.05f, 0.08f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        // 3) Prostor za nagradu
        {
            setRectVertices(HOLE_LEFT, HOLE_RIGHT, PRIZE_BOTTOM, PRIZE_TOP);
            glUniform4f(colorLoc, 0.12f, 0.12f, 0.16f, 1.0f);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        //// 4) Slot za zeton
        //{
        //    float left = -0.10f, right = 0.10f, bottom = -0.80f, top = -0.73f;
        //    setRectVertices(left, right, bottom, top);
        //    glUniform4f(colorLoc, 0.2f, 0.2f, 0.25f, 1.0f);
        //    glDrawArrays(GL_TRIANGLES, 0, 6);
        //}


        // 5) LAMPICA
        {
            float left = -0.05f, right = 0.05f, bottom = 0.45f, top = 0.55f;
            setRectVertices(left, right, bottom, top);


            int circleLoc = glGetUniformLocation(rectShader, "uIsCircle");
            glUniform1i(circleLoc, 1);  // ocu krug
            


            if (prizeAvailable) {
                // blink izmedju zelene i crvene
                if (lampGreenPhase) {
                    glUniform4f(colorLoc, 0.1f, 0.9f, 0.1f, 1.0f); // zelena
                }
                else {
                    glUniform4f(colorLoc, 0.9f, 0.1f, 0.1f, 1.0f); // crvena
                }
            }
            else if (gameActive) {
                // plava
                glUniform4f(colorLoc, 0.2f, 0.4f, 1.0f, 1.0f);
            }
            else {
                // siva
                glUniform4f(colorLoc, 0.3f, 0.3f, 0.35f, 1.0f);
            }

            glDrawArrays(GL_TRIANGLES, 0, 6);
            glUniform1i(circleLoc, 0); // reset za ostale pravougaonike
        }



        // 6) Igracke
        auto drawToy = [&](const Toy& t, float r, float g, float b)
            {
                if (t.state == ToyState::InBox ||
                    t.state == ToyState::Grabbed ||
                    t.state == ToyState::Falling ||
                    t.state == ToyState::InPrize)
                {
                    float left = t.x - t.halfSize;
                    float right = t.x + t.halfSize;
                    float bottom = t.y - t.halfSize;
                    float top = t.y + t.halfSize;

                    setRectVertices(left, right, bottom, top);
                    glUniform4f(colorLoc, r, g, b, 1.0f);
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                }
            };



        

        // 6.5) KONOPAC
        {
            // centar kandze (isti kao za sprite)
            float clawCenterY = clawY - ropeLength;

            // gornja tacka konopca = plafon kutije
            float ropeTop = BOX_TOP; // 0.4f

            // donja tacka konopca = vrh kandze (ne centar)
            float ropeBottom = clawCenterY + CLAW_HALF_H;

            // crtamo SAMO ako je kandza ispod plafona
            if (ropeBottom < ropeTop) {
                float left = clawX - ROPE_HALF_W;
                float right = clawX + ROPE_HALF_W;

                setRectVertices(left, right, ropeBottom, ropeTop);
                //glUniform4f(colorLoc, 0.2f, 0.4f, 1.0f, 1.0f); // plavi konopac
                glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, 1.0f); // plavi konopac

                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }


        glBindVertexArray(0);

        glUseProgram(spriteShader);

        
        // Jukebox (slot za zeton kao tekstura)
        {
            // centar slota je na x = 0, y = -0.765 (sredina starog pravougaonika)
            float slotCenterX = 0.0f;
            float slotCenterY = (-0.80f + -0.73f) * 0.5f; // -0.765f

            jukeboxSprite.x = slotCenterX;
            jukeboxSprite.y = slotCenterY;

            // sirina i visina (malo siri od starog kvadrata da lepse izgleda)
            jukeboxSprite.width = 0.15f;  // probaj pa posle po ukusu
            jukeboxSprite.height = 0.15f;  // ako ispadne „spljosten“, menjaj

            jukeboxSprite.draw(spriteShader);
        }



        // Toy1 svinja
        {
            toyPig.x = toy1.x;
            toyPig.y = toy1.y;
            toyPig.width = toy1.halfSize * 2.0f;
            toyPig.height = toy1.halfSize * 2.0f;
            toyPig.draw(spriteShader);
        }

        // Toy2 krava
        {
            toyCow.x = toy2.x;
            toyCow.y = toy2.y;
            toyCow.width = toy2.halfSize * 2.0f;
            toyCow.height = toy2.halfSize * 2.0f;
            toyCow.draw(spriteShader);
        }

        // 7) Kandza
        {
            Sprite& claw = (clawIsOpen ? clawOpen : clawClosed);

            claw.x = clawX;
            claw.y = clawY - ropeLength;
            claw.width = 0.10f;
            claw.height = 0.18f;

            claw.draw(spriteShader);



        //STUDENT LABELA 
        {
            // Pozicija u NDC x =-1 je skroz levo XD a 1 je skroz desno
            // Malo uvuceno od ivica
            studentLabel.x = -0.65f;   // levo
            studentLabel.y = 0.90f;   // gore

            // Velicina 
            studentLabel.width = 0.4f;   // sirina
            studentLabel.height = 0.12f;  // visina

            studentLabel.draw(spriteShader);
        }

        }
        //  CURSOR ZETON 
        {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            // Pretvori u NDC (-1 .. 1)
            float ndcX = static_cast<float>((mx / screenWidth) * 2.0 - 1.0);
            float ndcY = static_cast<float>(1.0 - (my / screenHeight) * 2.0);

            cursorToken.x = ndcX;
            cursorToken.y = ndcY;

            // velicina kursora
            cursorToken.width = 0.07f;
            cursorToken.height = 0.07f;

            cursorToken.draw(spriteShader);
        }

        // Swap + eventovi
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Frame limiter 75 FPS
        double frameTime = glfwGetTime() - now;
        if (frameTime < TARGET_FRAME_TIME) {
            double timeToSleep = TARGET_FRAME_TIME - frameTime;
            std::this_thread::sleep_for(std::chrono::duration<double>(timeToSleep));
        }
    }

    glDeleteBuffers(1, &rectVBO);
    glDeleteVertexArrays(1, &rectVAO);
    glDeleteProgram(rectShader);
    glDeleteProgram(spriteShader);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
