#include "SpaceLayer.h"

#include "GLController.h"
#include "GLTexture2.h"
#include "GLTexture2Loader.h"
#include "GLShaderLoader.h"

SpaceLayer::SpaceLayer(const Vector3f& initialEyePos) :
  InteractionLayer(initialEyePos, "shaders/solid"),
  m_PopupShader(Resource<GLShader>("shaders/transparent")),
  m_PopupTexture(Resource<GLTexture2>("level3_popup.png")) {
  m_Buffer.Create(GL_ARRAY_BUFFER);
  m_Buffer.Bind();
  m_Buffer.Allocate(NULL, 3*sizeof(float)*NUM_STARS, GL_DYNAMIC_DRAW);
  m_Buffer.Unbind();

  // Define popup text coordinates
  static const float edges[] = {
    -0.7f, -0.07f, -4.0f, 0, 0,
    -0.7f, +0.07f, -4.0f, 0, 1,
    +0.7f, -0.07f, -4.0f, 1, 0,
    +0.7f, +0.07f, -4.0f, 1, 1,
  };

  m_PopupBuffer.Create(GL_ARRAY_BUFFER);
  m_PopupBuffer.Bind();
  m_PopupBuffer.Allocate(edges, sizeof(edges), GL_STATIC_DRAW);
  m_PopupBuffer.Unbind();

  m_Buf = new float[NUM_STARS*3];
  InitPhysics();
}

SpaceLayer::~SpaceLayer() {
  delete[] m_Buf;
  m_Buffer.Destroy();
}


void SpaceLayer::Update(TimeDelta real_time_delta) {
  for (int i = 0; i < 2; i++) {
    UpdateAllPhysics();
  }
}

void SpaceLayer::Render(TimeDelta real_time_delta) const {
  RenderPopup();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glPointSize(2.0f);
  int start = SDL_GetTicks();

#if 0 // 12 ms per million particles
  glColor3f(1, 0, 0);

  glBegin(GL_POINTS);
  for (int i = 0; i < NUM_GALAXIES; i++) {
    switch (i) {
    case 0:
      glColor4f(1.0f, 1.0f, 0.8f, 0.6f*m_Alpha);
      break;
    case 1:
      glColor4f(1.0f, 0.8f, 1.0f, 0.6f*m_Alpha);
      break;
    case 2:
      glColor4f(0.8f, 1.0f, 1.0f, 0.6f*m_Alpha);
      break;
    }
    for (int j = 0; j < STARS_PER; j++) {
      int index = i*STARS_PER + j;
      glVertex3d(pos[index].x(), pos[index].y(), pos[index].z());
    }
  }
  glEnd();
#else // 4 ms per million particles
  for (int i = 0, j = 0; j < NUM_STARS; j++) {
    const Vector3& r = pos[j];
    m_Buf[i++] = r.x();
    m_Buf[i++] = r.y();
    m_Buf[i++] = r.z();
  }
  m_Shader->Bind();
  GLShaderMatrices::UploadUniforms(*m_Shader, m_ModelView.cast<double>(), m_Projection.cast<double>(), BindFlags::NONE);

  m_Buffer.Bind();
  glEnableVertexAttribArray(m_Shader->LocationOfAttribute("position"));
  glVertexAttribPointer(m_Shader->LocationOfAttribute("position"), 3, GL_FLOAT, GL_TRUE, 0, 0);

  m_Buffer.Write(m_Buf, 3*NUM_STARS*sizeof(float));
  glDrawArrays(GL_POINTS, 0, NUM_STARS);

  glDisableVertexAttribArray(m_Shader->LocationOfAttribute("position"));
  m_Buffer.Unbind();

  m_Shader->Unbind();
#endif
  //std::cout << __LINE__ << ":\t SDL_GetTicks() = " << (SDL_GetTicks() - start) << std::endl;
  glEnable(GL_DEPTH_TEST);
}

void SpaceLayer::RenderPopup() const {
  m_PopupShader->Bind();
  Matrix4x4f modelView = m_ModelView;
  modelView.block<3, 1>(0, 3) += modelView.block<3, 3>(0, 0)*m_EyePos;
  //modelView.block<3, 3>(0, 0) = Matrix3x3f::Identity();
  GLShaderMatrices::UploadUniforms(*m_PopupShader, modelView.cast<double>(), m_Projection.cast<double>(), BindFlags::NONE);

  glActiveTexture(GL_TEXTURE0 + 0);
  glUniform1i(m_PopupShader->LocationOfUniform("texture"), 0);

  m_PopupBuffer.Bind();
  glEnableVertexAttribArray(m_PopupShader->LocationOfAttribute("position"));
  glEnableVertexAttribArray(m_PopupShader->LocationOfAttribute("texcoord"));
  glVertexAttribPointer(m_PopupShader->LocationOfAttribute("position"), 3, GL_FLOAT, GL_TRUE, 5*sizeof(float), (GLvoid*)0);
  glVertexAttribPointer(m_PopupShader->LocationOfAttribute("texcoord"), 2, GL_FLOAT, GL_TRUE, 5*sizeof(float), (GLvoid*)(3*sizeof(float)));

  m_PopupTexture->Bind();
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  glBindTexture(GL_TEXTURE_2D, 0); // Unbind

  glDisableVertexAttribArray(m_PopupShader->LocationOfAttribute("position"));
  glDisableVertexAttribArray(m_PopupShader->LocationOfAttribute("texcoord"));
  m_PopupBuffer.Unbind();

  m_PopupShader->Unbind();
}

Vector3 SpaceLayer::GenerateVector(const Vector3& center, double radius) {
  const double rand_max(RAND_MAX);
  double dx, dy, dz;
  Vector3 dr;
  do {
    dx = (2.0*static_cast<double>(rand())/rand_max - 1.0)*radius;
    dy = (2.0*static_cast<double>(rand())/rand_max - 1.0)*radius;
    dz = (2.0*static_cast<double>(rand())/rand_max - 1.0)*radius;
    dr = Vector3(dx, dy, dz);
  } while (dr.squaredNorm() > radius*radius);
  return center + dr * (dr.squaredNorm()/(radius*radius)) * (dr.squaredNorm()/(radius*radius));
}

Vector3 SpaceLayer::InitialVelocity(double mass, const Vector3& normal, const Vector3& dr) {
  return std::sqrt(mass/dr.norm())*normal.cross(dr).normalized();
}

void SpaceLayer::InitPhysics() {
  pos.resize(NUM_STARS);
  vel.resize(NUM_STARS);

  for (int i = 0; i < NUM_GALAXIES; i++) {
    m_GalaxyPos[i] = GenerateVector(1.2*m_EyePos.cast<double>(), 1.0);
    if (i == 0) {
      m_GalaxyPos[i] = m_EyePos.cast<double>() + m_EyeView.transpose().cast<double>()*Vector3(0, -0.5, -1.0);
    }
    m_GalaxyVel[i] = GenerateVector(-1e-3*(m_GalaxyPos[i] - 1.1*m_EyePos.cast<double>()), 0.0004);
    m_GalaxyMass[i] = static_cast<double>(STARS_PER)*2e-10;
    m_GalaxyNormal[i] = GenerateVector(Vector3::Zero(), 1.0).normalized();

    for (int j = 0; j < STARS_PER; j++) {
      const size_t index = i*STARS_PER + j;
      Vector3 dr = GenerateVector(Vector3::Zero(), 0.5);

      // Project to disc
      dr -= 0.4*atan(dr.dot(m_GalaxyNormal[i])/0.5)*m_GalaxyNormal[i];

      pos[index] = m_GalaxyPos[i] + dr;
      vel[index] = m_GalaxyVel[i] + InitialVelocity(m_GalaxyMass[i], m_GalaxyNormal[i], dr);
    }
  }
}

void SpaceLayer::UpdateV(int type, const Vector3& p, Vector3& v, int galaxy) {
  if (galaxy == -1) {
    // Origin force
    //const Vector3 dr = m_EyePos.cast<double>() - p - 100*v;
    //v += 1e-6f*dr*dr.squaredNorm();
  } else if (galaxy < NUM_GALAXIES) {
    const Vector3 dr = m_GalaxyPos[galaxy] - p;
    v += m_GalaxyMass[galaxy]*dr.normalized()/(0.3e-3 + dr.squaredNorm());
  } else {
    double multiplier = 1.5e-4;
    const Vector3 dr = m_Tips[galaxy - NUM_GALAXIES].cast<double>() - (p + (0.1 + static_cast<double>(type)*0.0001)*v);
    v += multiplier*(dr)/(10e-3 + dr.squaredNorm());
  }
}

void SpaceLayer::UpdateAllPhysics() {
  // Update stars
  for (int i = 0; i < NUM_STARS; i++) {
    Vector3 tempV = vel[i];
    for (int j = 0; j < NUM_GALAXIES + m_Tips.size(); j++) {
      UpdateV(i, pos[i], tempV, j);
    }
    const Vector3 tempP = pos[i] + 0.667*tempV;
    for (int j = 0; j < NUM_GALAXIES + m_Tips.size(); j++) {
      UpdateV(i, tempP, vel[i], j);
    }
    pos[i] += 0.25*tempV + 0.75*vel[i];

    if ((pos[i] - m_EyePos.cast<double>()).squaredNorm() > 50) {
      pos[i] = m_EyePos.cast<double>() - 10*vel[i];
      vel[i].setZero();
    }
  }

  // Update galaxies
  for (int i = 0; i < NUM_GALAXIES; i++) {
    Vector3 tempV = m_GalaxyVel[i];
    for (int j = 0; j < NUM_GALAXIES + m_Tips.size(); j++) {
      if (i != j) { // Galaxy does not affect itself
        UpdateV(0, m_GalaxyPos[i], m_GalaxyVel[i], j);
      }
    }
    const Vector3 tempP = m_GalaxyPos[i] + 0.667*tempV;
    for (int j = 0; j < NUM_GALAXIES + m_Tips.size(); j++) {
      if (i != j) { // Galaxy does not affect itself
        UpdateV(0, tempP, m_GalaxyVel[i], j);
      }
    }
    m_GalaxyPos[i] += 0.25*tempV + 0.75*m_GalaxyVel[i];
  }
}

EventHandlerAction SpaceLayer::HandleKeyboardEvent(const SDL_KeyboardEvent &ev) {
  if (ev.type == SDL_KEYDOWN) {
    switch (ev.keysym.sym) {
    case SDLK_SPACE:
      InitPhysics();
      return EventHandlerAction::CONSUME;
    default:
      return EventHandlerAction::PASS_ON;
    }
  }
  return EventHandlerAction::PASS_ON;
}
