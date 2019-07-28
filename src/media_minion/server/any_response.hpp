#ifndef MEDIA_MINION_INCLUDE_GUARD_SERVER_ANY_RESPONSE_HPP_
#define MEDIA_MINION_INCLUDE_GUARD_SERVER_ANY_RESPONSE_HPP_

#include <gbBase/AnyInvocable.hpp>

#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/http/write.hpp>

#include <memory>

namespace media_minion::server {

class AnyResponse {
public:
    using WriteHandler = Ghulbus::AnyInvocable<void(boost::system::error_code const&, std::size_t)>;
private:
    class Concept {
    public:
        virtual ~Concept() = default;
        virtual void async_write(boost::asio::ip::tcp::socket& socket, WriteHandler handler) = 0;
        virtual bool need_eof() const = 0;
    };

    template<typename Body, typename Fields>
    class Model : public Concept {
    private:
        boost::beast::http::response<Body, Fields> m_response;
    public:
        Model(boost::beast::http::response<Body, Fields>&& r)
            :m_response(std::move(r))
        {}

        ~Model() override = default;

        void async_write(boost::asio::ip::tcp::socket& socket, WriteHandler handler) override
        {
            boost::beast::http::async_write(socket, m_response, std::move(handler));
        }

        bool need_eof() const override
        {
            return m_response.need_eof();
        }

        boost::beast::http::response<Body, Fields>* get() {
            return &m_response;
        }
    };
public:
    template<typename Body, typename Fields>
    AnyResponse(boost::beast::http::response<Body, Fields>&& response)
        :m_ptr(std::make_unique<Model<Body, Fields>>(std::move(response)))
    {}

    ~AnyResponse() = default;
    AnyResponse(AnyResponse&&) = default;
    AnyResponse& operator=(AnyResponse&&) = default;

    template<typename Body, typename Fields = boost::beast::http::fields>
    boost::beast::http::response<Body, Fields>* get() {
        auto concrete_model = dynamic_cast<Model<Body, Fields>*>(m_ptr.get());
        if (!concrete_model) { return nullptr; }
        return concrete_model->get();
    }

    void async_write(boost::asio::ip::tcp::socket& socket, WriteHandler handler)
    {
        m_ptr->async_write(socket, std::move(handler));
    }

    bool need_eof() const {
        return m_ptr->need_eof();
    }
private:
    std::unique_ptr<Concept> m_ptr;
};

}
#endif
