#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_ANY_REQUEST_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_ANY_REQUEST_HPP_

#include <boost/beast/http/message.hpp>

#include <memory>

namespace media_minion::server {

class AnyRequest {
private:
    class Concept {
    public:
        virtual ~Concept() = default;
    };

    template<typename Body, typename Fields>
    class Model : public Concept {
    private:
        boost::beast::http::request<Body, Fields> m_request;
    public:
        Model(boost::beast::http::request<Body, Fields>&& r)
            :m_request(std::move(r))
        {}

        boost::beast::http::request<Body, Fields>* get() {
            return &m_request;
        }
    };
public:
    template<typename Body, typename Fields>
    AnyRequest(boost::beast::http::request<Body, Fields>&& request)
        :m_ptr(std::make_unique<Model<Body, Fields>>(std::move(request)))
    {}

    ~AnyRequest() = default;
    AnyRequest(AnyRequest&&) = default;
    AnyRequest& operator=(AnyRequest&&) = default;

    template<typename Body, typename Fields = boost::beast::http::fields>
    boost::beast::http::request<Body, Fields>* get() {
        auto concrete_model = dynamic_cast<Model<Body, Fields>*>(m_ptr.get());
        if (!concrete_model) { return nullptr; }
        return concrete_model->get();
    }
private:
    std::unique_ptr<Concept> m_ptr;
};

}
#endif
