#include "scope.h"

using namespace yapvm::interpreter;


bool yapvm::interpreter::operator==(const ScopeEntry &a, const ScopeEntry &b) {
    return a.type_ == b.type_ && a.value_ == b.value_;
}

Scope::Scope() : parent_{ nullptr } {
    scope_.add(lst_exec_res, ScopeEntry{ nullptr, OBJECT });
}


bool Scope::add_object(std::string name, ManagedObject *value) {
    assert(value != nullptr);
    return scope_.add(std::move(name), ScopeEntry{static_cast<void *>(value), OBJECT});
}

bool Scope::add_function(std::string signature, FunctionDef *function) {
    assert(function != nullptr);
    return scope_.add(std::move(signature), ScopeEntry{static_cast<void *>(function), FUNCTION});
}

bool Scope::add_child_scope(std::string name, Scope *subscope) {
    assert(subscope != nullptr);
    subscope->parent_ = this;
    return scope_.add(std::move(name), ScopeEntry{static_cast<void *>(subscope), SCOPE});
}

bool Scope::add(std::string name, ScopeEntry entry) { return scope_.add(std::move(name), entry); }


void Scope::change(const std::string &name, ScopeEntry new_entry) {
    std::optional<std::reference_wrapper<ScopeEntry>> opt_from_kv = scope_[name];
    if (!opt_from_kv.has_value()) {
        add(name, new_entry);
        return;
    }

    ScopeEntry &entry = opt_from_kv.value().get();
    entry = new_entry;
}

void Scope::del(const std::string &name) {
    scope_.del(name);
}

void Scope::store_last_exec_res(const std::string &name) { change(name, get(lst_exec_res).value()); }

void Scope::update_last_exec_res(ManagedObject *value) {
    change(Scope::lst_exec_res, ScopeEntry{ value, OBJECT });
}


ScopeEntry Scope::name_lookup(const std::string &name) {
    Scope *checkee = this;
    do {
        std::optional<ScopeEntry> curr_scope_lookup_res = checkee->get(name);
        if (curr_scope_lookup_res.has_value()) {
            return curr_scope_lookup_res.value();
        }
        if (checkee->parent_ == nullptr) {
            throw std::runtime_error("Scope: cannot find name [" + name + "] in program scope");
        }
        checkee = checkee->parent_;
    } while (true);
}


std::string Scope::scope_entry_function_name(const std::string &name) { return "__yapvm_inner_function_" + name; }


std::string Scope::scope_entry_call_subscope_name(const std::string &name) {
    return "__yapvm_inner_call_scope_" + name;
}


std::string Scope::scope_entry_thread_name(size_t id) {
    return "__yapvm_thread_scope_" + std::to_string(id);
}


ManagedObject *Scope::get_object(const std::string &name) {
    std::optional<std::reference_wrapper<ScopeEntry>> opt_from_kv = scope_[name];
    if (opt_from_kv == std::nullopt) return nullptr; 
    ScopeEntry se_ref = opt_from_kv.value().get();
    return static_cast<ManagedObject *>(se_ref.value_);
}

FunctionDef *Scope::get_function(const std::string &signature) {
    std::optional<std::reference_wrapper<ScopeEntry>> opt_from_kv = scope_[signature];
    if (opt_from_kv == std::nullopt) return nullptr; 
    ScopeEntry se_ref = opt_from_kv.value().get();
    return static_cast<FunctionDef *>(se_ref.value_);   
}

std::optional<ScopeEntry> Scope::get(const std::string &name) {
    std::optional<std::reference_wrapper<ScopeEntry>> opt_from_kv = scope_[name];
    if (opt_from_kv == std::nullopt) return std::nullopt;
    ScopeEntry se_ref = opt_from_kv.value().get();
    return std::optional<ScopeEntry>{ se_ref };
}

std::vector<Scope *> Scope::get_all_children() const {
    std::vector<Scope *> children;
    for (std::vector<ScopeEntry *> fields = scope_.get_live_entries_values(); ScopeEntry *f : fields) {
        if (f->type_ == SCOPE) children.push_back(static_cast<Scope *>(f->value_));
    }
    return children;
}

std::vector<ManagedObject *> yapvm::interpreter::Scope::get_all_objects() const {
    std::vector<ScopeEntry *> values = scope_.get_live_entries_values();
    std::vector<ManagedObject *> res;
    for (auto se: values) {
        if (se->value_ == nullptr)
            continue;

        if (se->type_ == OBJECT) {
            res.push_back(static_cast<ManagedObject *>(se->value_));
        } else if (se->type_ == SCOPE) {
            auto scoped_objs = (static_cast<Scope *>(se->value_))->get_all_objects();
            res.insert(res.end(), scoped_objs.begin(), scoped_objs.end());
        }
    }
    return res;
}

std::vector<std::pair<std::string, ScopeEntry>> Scope::get_all() const {
    std::vector<std::pair<std::string *, ScopeEntry *>> all = scope_.get_live_entries();
    std::vector<std::pair<std::string, ScopeEntry>> ret;
    for (std::pair<std::string *, ScopeEntry *> &e : all) {
        ret.emplace_back(*e.first, *e.second);
    }
    return ret;
}

Scope *Scope::parent() const {
    return parent_;
}
