#pragma once

#include "RepositoryTemplate.h"

#include <utility>

template<typename Query, typename Result, typename Handler>
class RepositoryFunctionOperation final : public RepositoryTemplate<Query, Result> {
public:
    explicit RepositoryFunctionOperation(Handler handler)
        : m_handler(std::move(handler))
    {
    }

private:
    Result doRequest(const Query& query) const override
    {
        return m_handler(query);
    }

    Handler m_handler;
};

