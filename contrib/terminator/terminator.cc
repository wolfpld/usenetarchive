//
//  terminator.cc
//  terminator
//
//  @author freiz
//  @email freizsu@gmail.com
//

#include "terminator.h"

const node Terminator::DEFALULT_NODE_VALUE = {
  0.0,
  2.0,
  1.0,
  0,
  0,
  1.0,
  0.0,
  0.0,
  1.0,
  0,
  0,
  0.0,
  0,
  0
};

const std::string Terminator::IDENTIFIER_CLASSIFIER_WEIGHTS = "classifier:weights";
const std::string Terminator::IDENTIFIER_TOTAL_SPAM = "classifier:total:spam";
const std::string Terminator::IDENTIFIER_TOTAL_HAM = "classifier:total:ham";

Terminator::Terminator(std::string db_path, size_t mem_cache) {
  this->db_path_ = db_path;
  this->mem_cache_ = mem_cache;
  if (this->InitDB(db_path, mem_cache) != true) {
    printf("Errors in initialization of kyotocabinet db file, check the file path and the library");
    exit(EXIT_FAILURE);
  }
  this->PrepareMetaData();
  // Real classifier in use
  this->classifier_ = new TerminatorClassifierOWV(this->classifier_weights_);
}

Terminator::~Terminator() {
  delete(this->cache_node_);
  db_.set(Terminator::IDENTIFIER_TOTAL_SPAM.c_str(), Terminator::IDENTIFIER_TOTAL_SPAM.size(),
          (char*)&TerminatorClassifierBase::TotalSpam, sizeof(TerminatorClassifierBase::TotalSpam));
  db_.set(Terminator::IDENTIFIER_TOTAL_HAM.c_str(), Terminator::IDENTIFIER_TOTAL_HAM.size(),
          (char*)&TerminatorClassifierBase::TotalHam, sizeof(TerminatorClassifierBase::TotalHam));
  db_.set(Terminator::IDENTIFIER_CLASSIFIER_WEIGHTS.c_str(), Terminator::IDENTIFIER_CLASSIFIER_WEIGHTS.size(),
          (char*)this->classifier_weights_, sizeof(this->classifier_weights_));
  db_.close();
}

double Terminator::Predict(std::string email_content) {
  std::map<std::string, node> weights;
  this->Vectorization(email_content, weights);
  return this->classifier_->Predict(weights);
}

void Terminator::Train(std::string email_content, bool is_spam) {
  std::map<std::string, node> weights;
  this->Vectorization(email_content, weights);
  this->classifier_->Train(weights, is_spam);
  SaveWeights(weights);
}


bool Terminator::InitDB(std::string db_path, size_t mem_cache) {
  db_.tune_map(mem_cache);
  return db_.open(db_path_, kyotocabinet::HashDB::OWRITER | kyotocabinet::HashDB::OCREATE);
}

void Terminator::PrepareMetaData() {
  // Allocate mem for node cache
  this->cache_node_ = new node();
  unsigned long long total_spam;
  unsigned long long total_ham;
  unsigned i;
  double classifier_weights[CLASSIFIER_NUMBER];
  // Read Total Spam
  if (db_.get(Terminator::IDENTIFIER_TOTAL_SPAM.c_str(), Terminator::IDENTIFIER_TOTAL_SPAM.size(),
              (char*)&total_spam, sizeof(unsigned long long))
      == -1) {
    TerminatorClassifierBase::TotalSpam = 0;
  } else {
    TerminatorClassifierBase::TotalSpam = total_spam;
  }
  // Read Total Ham
  if (db_.get(Terminator::IDENTIFIER_TOTAL_HAM.c_str(), Terminator::IDENTIFIER_TOTAL_HAM.size(),
              (char*)&total_ham, sizeof(unsigned long long))
      == -1) {
    TerminatorClassifierBase::TotalHam = 0;
  } else {
    TerminatorClassifierBase::TotalHam = total_ham;
  }
  // Read the weights of each classifier
  if (db_.get(Terminator::IDENTIFIER_CLASSIFIER_WEIGHTS.c_str(), Terminator::IDENTIFIER_CLASSIFIER_WEIGHTS.size(),
              (char*)classifier_weights, sizeof(classifier_weights))
      == -1) {
    for (i = 0; i < CLASSIFIER_NUMBER; i++) this->classifier_weights_[i] = 1;
  } else {
    for (i = 0; i < CLASSIFIER_NUMBER; i++) this->classifier_weights_[i] = classifier_weights[i];
  }
}

void Terminator::Vectorization(std::string email_content, std::map<std::string, node>& weights) {
  size_t len = email_content.length();
  len = len <= MAX_READ_LEN ? len : MAX_READ_LEN;
  for (unsigned i = 0; i <= len - NGRAM; i++) {
    std::string feature = email_content.substr(i, NGRAM);
    if (db_.get(feature.c_str(), feature.size(), (char*) cache_node_, sizeof(node))
        == -1) {
      weights[feature] = Terminator::DEFALULT_NODE_VALUE;
    } else {
      weights[feature] = *(cache_node_);
    }
  }
}

void Terminator::SaveWeights(std::map<std::string, node> &weights) {
  std::map<std::string, node>::iterator iter;
  for (iter = weights.begin(); iter != weights.end(); ++iter) {
    this->db_.set(iter->first.c_str(), iter->first.size(), (char*)&iter->second, sizeof(node));
  }
}

