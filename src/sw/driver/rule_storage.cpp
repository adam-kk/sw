// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "rule_storage.h"

#include "rule.h"

namespace sw
{

RuleStorage::RuleStorage() = default;
RuleStorage::~RuleStorage() = default;

void RuleStorage::push(const String &name, std::unique_ptr<IRule> r)
{
    rules[name].emplace(std::move(r));
}

std::unique_ptr<IRule> RuleStorage::pop(const String &name)
{
    if (!contains(name))
        return {};
    std::unique_ptr<IRule> r = std::move(rules[name].top());
    rules[name].pop();
    return r;
}

bool RuleStorage::contains(const String &name) const
{
    return rules.contains(name);
}

void RuleStorage::clear()
{
    rules.clear();
}

void RuleStorage::clear(const String &name)
{
    rules.erase(name);
}

Commands RuleStorage::getCommands() const
{
    Commands c;
    for (auto &[_,s] : rules)
        c.merge(s.top()->getCommands());
    return c;
}

Commands RuleSystem::getRuleCommands() const
{
    return rules.getCommands();
}

} // namespace sw
