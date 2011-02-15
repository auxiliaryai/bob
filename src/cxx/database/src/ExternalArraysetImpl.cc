/**
 * @file database/src/ExternalArraysetImpl.cc
 * @author <a href="mailto:andre.anjos@idiap.ch">Andre Anjos</a> 
 *
 * @brief Implements reading and writing data to arrays in files.
 */

#include <boost/filesystem.hpp>

#include "database/ExternalArraysetImpl.h"
#include "database/InlinedArraysetImpl.h"
#include "database/ArraysetCodecRegistry.h"

namespace tdd = Torch::database::detail;

tdd::ExternalArraysetImpl::ExternalArraysetImpl(const std::string& filename, 
    const std::string& codecname)
  : m_filename(filename)
{
  //the next instructions will raise an exception if the code is not found.
  if (codecname.size()) {
    m_codec = Torch::database::ArraysetCodecRegistry::getCodecByName(codecname);
  }
  else {
    m_codec = Torch::database::ArraysetCodecRegistry::getCodecByExtension(filename);
  }
  reloadSpecification();
}

tdd::ExternalArraysetImpl::~ExternalArraysetImpl() {}

void tdd::ExternalArraysetImpl::reloadSpecification() {
  m_codec->peek(m_filename, m_elementtype, m_ndim, m_shape, m_samples);
}

void tdd::ExternalArraysetImpl::move(const std::string& filename,
    const std::string& codecname) {
  boost::shared_ptr<const Torch::database::ArraysetCodec> newcodec;
  if (codecname.size())
    newcodec = Torch::database::ArraysetCodecRegistry::getCodecByName(codecname);
  else
    newcodec = Torch::database::ArraysetCodecRegistry::getCodecByExtension(filename);
  if (newcodec == m_codec) { //just rename the file
    boost::filesystem::path path(m_filename);
    boost::filesystem::rename(boost::filesystem::path(m_filename),
        boost::filesystem::path(filename));
    m_filename = filename;
  }
  else { //the user wants to re-write it in a different format.
    newcodec->save(filename, m_codec->load(m_filename));
    boost::filesystem::remove(boost::filesystem::path(m_filename));
    m_codec = newcodec;
  }
  reloadSpecification();
}

Torch::database::Array tdd::ExternalArraysetImpl::operator[] (size_t id) const {
  return m_codec->load(m_filename, id);
}

void tdd::ExternalArraysetImpl::checkCompatibility(const Torch::database::Array& array) const {
  if (m_elementtype != Torch::core::array::t_unknown) {
    if (array.getElementType() != m_elementtype) throw Torch::database::TypeError();
    if (array.getNDim() != m_ndim) throw Torch::database::DimensionError();
    for (size_t i=0; i<m_ndim; ++i)
      if (array.getShape()[i] != m_shape[i]) throw Torch::database::DimensionError();
  }
}

size_t tdd::ExternalArraysetImpl::add
(boost::shared_ptr<const Torch::database::Array> array) {
  return add(*array.get()); 
}

size_t tdd::ExternalArraysetImpl::add(const Torch::database::Array& array) {
  checkCompatibility(array);

  m_codec->append(m_filename, array);
  reloadSpecification();
  return m_samples;
}

void tdd::ExternalArraysetImpl::extend(const tdd::InlinedArraysetImpl& set) {
  for(std::list<boost::shared_ptr<Torch::database::Array> >::const_iterator it= set.arrays().begin(); it != set.arrays().end(); ++it) {
    add(*it);
  }
  reloadSpecification();
}

void tdd::ExternalArraysetImpl::remove(size_t id) {
  Torch::core::array::ElementType eltype;
  size_t ndim;
  size_t shape[Torch::core::array::N_MAX_DIMENSIONS_ARRAY];
  size_t samples;
  m_codec->peek(m_filename, eltype, ndim, shape, samples);
  if (id > samples) throw Torch::database::IndexError();
  
  //loads the file and rewrite it.
  tdd::InlinedArraysetImpl data = get();
  data.remove(id);
  set(data);
}

void tdd::ExternalArraysetImpl::remove(boost::shared_ptr<const Torch::database::Array> array) {
  remove(array->getId());
}

void tdd::ExternalArraysetImpl::remove(const Torch::database::Array& array) {
  remove(array.getId());
}

tdd::InlinedArraysetImpl tdd::ExternalArraysetImpl::get() const {
  return m_codec->load(m_filename);
}

void tdd::ExternalArraysetImpl::set(const InlinedArraysetImpl& set) {
  m_codec->save(m_filename, set);
  reloadSpecification();
}

bool tdd::ExternalArraysetImpl::exists(size_t id) const {
  return (id <= getNSamples());
}
