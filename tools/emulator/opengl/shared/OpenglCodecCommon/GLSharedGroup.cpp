#include "GLSharedGroup.h"

/**** BufferData ****/

BufferData::BufferData() : m_size(0) {};
BufferData::BufferData(GLsizeiptr size, void * data) : m_size(size) 
{
    void * buffer = NULL;
    if (size>0) buffer = m_fixedBuffer.alloc(size);
    if (data) memcpy(buffer, data, size);
}

/**** ProgramData ****/
ProgramData::ProgramData() : m_numIndexes(0),
                             m_initialized(false),
                             m_locShiftWAR(false)
{
    m_Indexes = NULL;
}

void ProgramData::initProgramData(GLuint numIndexes)
{
    m_initialized = true;
    m_numIndexes = numIndexes;
    delete[] m_Indexes;
    m_Indexes = new IndexInfo[numIndexes];
    m_locShiftWAR = false;
}

bool ProgramData::isInitialized()
{
    return m_initialized;
}

ProgramData::~ProgramData()
{
    delete[] m_Indexes;
    m_Indexes = NULL;
}

void ProgramData::setIndexInfo(GLuint index, GLint base, GLint size, GLenum type)
{   
    if (index>=m_numIndexes)
        return;
    m_Indexes[index].base = base;
    m_Indexes[index].size = size;
    m_Indexes[index].type = type;
}

GLuint ProgramData::getIndexForLocation(GLint location)
{
    GLuint i=0;
    for (i=0;i<m_numIndexes;++i)
    {
        GLint low = m_Indexes[i].base;;
        GLint high = low + m_Indexes[i].size;
        if (location >= low && location < high)
            break;
    }
    return i;
}

GLenum ProgramData::getTypeForLocation(GLint location)
{
    GLuint index = getIndexForLocation(location);
    if (index<m_numIndexes) {
        return m_Indexes[index].type;
    }
    return 0;
}

void ProgramData::setupLocationShiftWAR()
{
    m_locShiftWAR = false;
    for (int i=0; i<m_numIndexes; i++) {
        if (0 != (m_Indexes[i].base & 0xffff)) {
            return;
        }
    }
    m_locShiftWAR = true;
}

GLint ProgramData::locationWARHostToApp(GLint hostLoc)
{
    if (m_locShiftWAR) return hostLoc>>16;
    else return hostLoc;
}

GLint ProgramData::locationWARAppToHost(GLint appLoc)
{
    if (m_locShiftWAR) return appLoc<<16;
    else return appLoc;
}


/***** GLSharedGroup ****/

GLSharedGroup::GLSharedGroup() :
    m_buffers(android::DefaultKeyedVector<GLuint, BufferData*>(NULL)),
    m_programs(android::DefaultKeyedVector<GLuint, ProgramData*>(NULL)),
    m_shaders(android::List<GLuint>())
{
}

GLSharedGroup::~GLSharedGroup()
{
    m_buffers.clear();
    m_programs.clear();
}

BufferData * GLSharedGroup::getBufferData(GLuint bufferId)
{
    android::AutoMutex _lock(m_lock);
    return m_buffers.valueFor(bufferId);    
}

void GLSharedGroup::addBufferData(GLuint bufferId, GLsizeiptr size, void * data)
{
    android::AutoMutex _lock(m_lock);
    m_buffers.add(bufferId, new BufferData(size, data));
}

void GLSharedGroup::updateBufferData(GLuint bufferId, GLsizeiptr size, void * data)
{
    android::AutoMutex _lock(m_lock);
    m_buffers.replaceValueFor(bufferId, new BufferData(size, data));
}

GLenum GLSharedGroup::subUpdateBufferData(GLuint bufferId, GLintptr offset, GLsizeiptr size, void * data)
{
    android::AutoMutex _lock(m_lock);
    BufferData * buf = m_buffers.valueFor(bufferId);
    if ((!buf) || (buf->m_size < offset+size) || (offset < 0) || (size<0)) return GL_INVALID_VALUE; 

    //it's safe to update now
    memcpy((char*)buf->m_fixedBuffer.ptr() + offset, data, size);
    return GL_NO_ERROR; 
}

void GLSharedGroup::deleteBufferData(GLuint bufferId)
{
    android::AutoMutex _lock(m_lock);
    m_buffers.removeItem(bufferId);
}

void GLSharedGroup::addProgramData(GLuint program)
{
    android::AutoMutex _lock(m_lock);
    ProgramData *pData = m_programs.valueFor(program);
    if (pData) 
    {   
        m_programs.removeItem(program);
        delete pData;
    }

    m_programs.add(program,new ProgramData());
}

void GLSharedGroup::initProgramData(GLuint program, GLuint numIndexes)
{
    android::AutoMutex _lock(m_lock);
    ProgramData *pData = m_programs.valueFor(program);
    if (pData)
    {
        pData->initProgramData(numIndexes);
    }
}

bool GLSharedGroup::isProgramInitialized(GLuint program)
{
    android::AutoMutex _lock(m_lock);
    ProgramData* pData = m_programs.valueFor(program);
    if (pData) 
    {
        return pData->isInitialized();
    }
    return false;
}

void GLSharedGroup::deleteProgramData(GLuint program)
{
    android::AutoMutex _lock(m_lock);
    ProgramData *pData = m_programs.valueFor(program);
    if (pData)
        delete pData;
    m_programs.removeItem(program); 
}

void GLSharedGroup::setProgramIndexInfo(GLuint program, GLuint index, GLint base, GLint size, GLenum type)
{
    android::AutoMutex _lock(m_lock);
    ProgramData* pData = m_programs.valueFor(program);
    if (pData)
    {
        pData->setIndexInfo(index,base,size,type);
    }
}

GLenum GLSharedGroup::getProgramUniformType(GLuint program, GLint location)
{
    android::AutoMutex _lock(m_lock);
    ProgramData* pData = m_programs.valueFor(program);
    GLenum type=0;
    if (pData) 
    {
        type = pData->getTypeForLocation(location);
    }
    return type;
}

bool  GLSharedGroup::isProgram(GLuint program)
{
    android::AutoMutex _lock(m_lock);
    ProgramData* pData = m_programs.valueFor(program);
    return (pData!=NULL);
}

void GLSharedGroup::setupLocationShiftWAR(GLuint program)
{
    android::AutoMutex _lock(m_lock);
    ProgramData* pData = m_programs.valueFor(program);
    if (pData) pData->setupLocationShiftWAR();
}

GLint GLSharedGroup::locationWARHostToApp(GLuint program, GLint hostLoc)
{
    android::AutoMutex _lock(m_lock);
    ProgramData* pData = m_programs.valueFor(program);
    if (pData) return pData->locationWARHostToApp(hostLoc);
    else return hostLoc;
}

GLint GLSharedGroup::locationWARAppToHost(GLuint program, GLint appLoc)
{
    android::AutoMutex _lock(m_lock);
    ProgramData* pData = m_programs.valueFor(program);
    if (pData) return pData->locationWARAppToHost(appLoc);
    else return appLoc;
}


void  GLSharedGroup::addShaderData(GLuint shader)
{
    android::AutoMutex _lock(m_lock);
    m_shaders.push_front(shader);
    
}
bool  GLSharedGroup::isShader(GLuint shader)
{
    android::AutoMutex _lock(m_lock);
    android::List<GLuint>::iterator iter;
    iter = m_shaders.begin();
    while (iter!=m_shaders.end())
    {
        if (*iter==shader)
            return true;
        iter++;
    }
    return false;
}
void  GLSharedGroup::deleteShaderData(GLuint shader)
{
    android::AutoMutex _lock(m_lock);
    android::List<GLuint>::iterator iter;
    iter = m_shaders.begin();
    while (iter!=m_shaders.end())
    {
        if (*iter==shader)
        {
            m_shaders.erase(iter);
            return;
        }
        iter++;
    }
}
