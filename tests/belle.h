    class Server
    {
    public:

        template<typename Derived>
        class Websocket_Base : public Websocket_Session
        {
            Derived& derived()
            {
                return static_cast<Derived&>(*this);
            }

        public:

            Websocket_Base(net::io_context& io_, std::shared_ptr<Attr> const attr_,
                           Request&& req_, fns_on_websocket const& on_websocket_) :
                    _attr {attr_},
                    _ctx {static_cast<Derived&>(*this), std::move(req_), _attr->channels},
                    _on_websocket {on_websocket_},
                    _strand {io_.get_executor()}
            {
            }

            ~Websocket_Base()
            {
                // leave channel
                _attr->channels.at(_ctx.req.path().at(0)).leave(derived());

                if (_on_websocket.end)
                {
                    try
                    {
                        // run user function
                        _on_websocket.end(_ctx);
                    }
                    catch (...)
                    {
                        this->handle_error();
                    }
                }

                if (_attr->on_websocket_disconnect)
                {
                    try
                    {
                        // run user function
                        _attr->on_websocket_disconnect(_ctx);
                    }
                    catch (...)
                    {
                        this->handle_error();
                    }
                }
            }

            void send(std::string const&& str_)
            {
                auto const pstr = std::make_shared<std::string const>(std::move(str_));
                _que.emplace_back(pstr);

                if (_que.size() > 1)
                {
                    return;
                }

                derived().socket().async_write(net::buffer(*_que.front()),
                                               [self = derived().shared_from_this()](error_code ec, std::size_t bytes)
                                               {
                                                   self->on_write(ec, bytes);
                                               }
                );
            }

            void handle_error()
            {
                if (_attr->on_websocket_error)
                {
                    try
                    {
                        // run user function
                        _attr->on_websocket_error(_ctx);
                    }
                    catch (...)
                    {
                    }
                }
            }

            void do_accept()
            {
                derived().socket().control_callback(
                        [this](websocket::frame_type type, boost::beast::string_view data)
                        {
                            this->on_control_callback(type, data);
                        }
                );

                derived().socket().async_accept_ex(_ctx.req,
                                                   [&](auto& res)
                                                   {
                                                       for (auto const& e : _attr->http_headers)
                                                       {
                                                           res.insert(e.name_string(), e.value());
                                                       }
                                                   },
                                                   net::bind_executor(_strand,
                                                                      [self = derived().shared_from_this()](error_code ec)
                                                                      {
                                                                          self->on_accept(ec);
                                                                      }
                                                   )
                );
            }

            void on_accept(error_code ec_)
            {
                if (ec_ == net::error::operation_aborted)
                {
                    return;
                }

                if (ec_)
                {
                    // TODO log here
                    return;
                }

                // join channel
                if (_attr->channels.find(_ctx.req.path().at(0)) == _attr->channels.end())
                {
                    _attr->channels[_ctx.req.path().at(0)] = Channel();
                }
                _attr->channels.at(_ctx.req.path().at(0)).join(derived());

                if (_attr->on_websocket_connect)
                {
                    try
                    {
                        // run user function
                        _attr->on_websocket_connect(_ctx);
                    }
                    catch (...)
                    {
                        this->handle_error();
                    }
                }

                if (_on_websocket.begin)
                {
                    try
                    {
                        // run user function
                        _on_websocket.begin(_ctx);
                    }
                    catch (...)
                    {
                        this->handle_error();
                    }
                }

                this->do_read();
            }

            void on_control_callback(websocket::frame_type type_, boost::beast::string_view data_)
            {
                boost::ignore_unused(type_, data_);
            }

            void do_read()
            {
                derived().socket().async_read(_buf,
                                              net::bind_executor(_strand,
                                                                 [self = derived().shared_from_this()](error_code ec, std::size_t bytes)
                                                                 {
                                                                     self->on_read(ec, bytes);
                                                                 }
                                              )
                );
            }

            void on_read(error_code ec_, std::size_t bytes_)
            {
                boost::ignore_unused(bytes_);

                // socket closed by the timer
                if (ec_ == net::error::operation_aborted)
                {
                    return;
                }

                // socket closed
                if (ec_ == websocket::error::closed)
                {
                    return;
                }

                if (ec_)
                {
                    // TODO log here
                    return;
                }

                if (_on_websocket.data)
                {
                    try
                    {
                        _ctx.msg = boost::beast::buffers_to_string(_buf.data());

                        // run user function
                        _on_websocket.data(_ctx);
                    }
                    catch (...)
                    {
                        handle_error();
                    }
                }

                // clear the request object
                _ctx.req.clear();

                // clear the buffers
                _buf.consume(_buf.size());

                this->do_read();
            }

            void on_write(error_code ec_, std::size_t bytes_)
            {
                boost::ignore_unused(bytes_);

                // happens when the timer closes the socket
                if (ec_ == net::error::operation_aborted)
                {
                    return;
                }

                if (ec_)
                {
                    // TODO log here
                    return;
                }

                // remove sent message from the queue
                _que.pop_front();

                if (_que.empty())
                {
                    return;
                }

                derived().socket().async_write(net::buffer(*_que.front()),
                                               [self = derived().shared_from_this()](error_code ec, std::size_t bytes)
                                               {
                                                   self->on_write(ec, bytes);
                                               }
                );
            }

            std::shared_ptr<Attr> const _attr;
            Websocket_Ctx _ctx;
            fns_on_websocket const& _on_websocket;
            net::strand<net::io_context::executor_type> _strand;
            boost::beast::multi_buffer _buf;
            std::deque<std::shared_ptr<std::string const>> _que {};
        }; // class Websocket_Base

        class Websocket :
                public Websocket_Base<Websocket>,
                public std::enable_shared_from_this<Websocket>
        {
        public:

            Websocket(tcp::socket&& socket_, std::shared_ptr<Attr> const attr_,
                      Request&& req_, fns_on_websocket const& on_websocket_) :
                    Websocket_Base<Websocket> {socket_.get_executor().context(), attr_,
                                               std::move(req_), on_websocket_},
                    _socket {std::move(socket_)}
            {
            }

            ~Websocket()
            {
            }

            websocket::stream<tcp::socket>& socket()
            {
                return _socket;
            }

            void run()
            {
                this->do_accept();
            }

            void do_timeout()
            {
                this->do_shutdown();
            }

            void do_shutdown()
            {
                _socket.async_close(websocket::close_code::normal,
                                    net::bind_executor(this->_strand,
                                                       [self = this->shared_from_this()](error_code ec)
                                                       {
                                                           self->on_shutdown(ec);
                                                       }
                                    )
                );
            }

            void on_shutdown(error_code ec_)
            {
                if (ec_)
                {
                    // TODO log here
                    return;
                }
            }

        private:

            websocket::stream<tcp::socket> _socket;
        }; // class Websocket

#ifdef OB_BELLE_CONFIG_SSL_ON
        class Websockets :
                public Websocket_Base<Websockets>,
                public std::enable_shared_from_this<Websockets>
        {
        public:

            Websockets(Detail::ssl_stream<tcp::socket>&& socket_, std::shared_ptr<Attr> const attr_,
                       Request&& req_, fns_on_websocket const& on_websocket_) :
                    Websocket_Base<Websockets> {socket_.get_executor().context(), attr_,
                                                std::move(req_), on_websocket_},
                    _socket {std::move(socket_)}
            {
            }

            ~Websockets()
            {
            }

            websocket::stream<Detail::ssl_stream<tcp::socket>>& socket()
            {
                return _socket;
            }

            void run()
            {
                this->do_accept();
            }

            void do_timeout()
            {
                this->do_shutdown();
            }

            void do_shutdown()
            {
                _socket.async_close(websocket::close_code::normal,
                                    net::bind_executor(this->_strand,
                                                       [self = this->shared_from_this()](error_code ec)
                                                       {
                                                           self->on_shutdown(ec);
                                                       }
                                    )
                );
            }

            void on_shutdown(error_code ec_)
            {
                if (ec_)
                {
                    // TODO log here
                    return;
                }
            }

        private:

            websocket::stream<Detail::ssl_stream<tcp::socket>> _socket;
        }; // class Websockets
#endif // OB_BELLE_CONFIG_SSL_ON

        template<typename Derived, typename Websocket_Type>
        class Http_Base
        {
            Derived& derived()
            {
                return static_cast<Derived&>(*this);
            }

        public:

            Http_Base(net::io_context& io_, std::shared_ptr<Attr> const attr_) :
                    _strand {io_.get_executor()},
                    _timer {io_, (std::chrono::steady_clock::time_point::max)()},
                    _attr {attr_}
            {
            }

            ~Http_Base()
            {
            }

// TODO remove shim once visual studio supports generic lambdas
#ifdef _MSC_VER
            template<typename Self, typename Res>
    static void constexpr send(Self self, Res&& res)
#else
            // generic lambda for sending different types of responses
            static auto constexpr send = [](auto self, auto&& res) -> void
#endif // _MSC_VER
            {
                using item_type = std::remove_reference_t<decltype(res)>;

                auto ptr = std::make_shared<item_type>(std::move(res));
                self->_res = ptr;

                http::async_write(self->derived().socket(), *ptr,
                                  net::bind_executor(self->_strand,
                                                     [self, close = ptr->need_eof()]
                                                             (error_code ec, std::size_t bytes)
                                                     {
                                                         self->on_write(ec, bytes, close);
                                                     }
                                  )
                );
            };

            int serve_static()
            {
                if (! _attr->http_static || _attr->public_dir.empty())
                {
                    return 404;
                }

                if ((_ctx.req.method() != http::verb::get) && (_ctx.req.method() != http::verb::head))
                {
                    return 404;
                }

                std::string path {_attr->public_dir + _ctx.req.target().to_string()};

                if (path.back() == '/')
                {
                    path += _attr->index_file;
                }

                error_code ec;
                http::file_body::value_type body;
                body.open(path.data(), beast::file_mode::scan, ec);

                if (ec)
                {
                    return 404;
                }

                // head request
                if (_ctx.req.method() == http::verb::head)
                {
                    http::response<http::empty_body> res {};
                    res.base() = http::response_header<>(_attr->http_headers);
                    res.version(_ctx.req.version());
                    res.keep_alive(_ctx.req.keep_alive());
                    res.content_length(body.size());
                    res.set(Header::content_type, mime_type(path));
                    send(derived().shared_from_this(), std::move(res));
                    return 0;
                }

                // get request
                auto const size = body.size();
                http::response<http::file_body> res {
                        std::piecewise_construct,
                        std::make_tuple(std::move(body)),
                        std::make_tuple(_attr->http_headers)
                };
                res.version(_ctx.req.version());
                res.keep_alive(_ctx.req.keep_alive());
                res.content_length(size);
                res.set(Header::content_type, mime_type(path));
                send(derived().shared_from_this(), std::move(res));
                return 0;
            }

            int serve_dynamic()
            {
                if (! _attr->http_dynamic || _attr->http_routes.empty())
                {
                    return 404;
                }

                // regex variables
                std::smatch rx_match {};
                std::regex_constants::syntax_option_type const rx_opts {std::regex::ECMAScript};
                std::regex_constants::match_flag_type const rx_flgs {std::regex_constants::match_not_null};

                // the request path
                std::string path {_ctx.req.target().to_string()};

                // separate the query parameters
                auto params = Detail::split(path, "?", 1);
                path = params.at(0);

                // iterate over routes
                for (auto const& regex_method : _attr->http_routes)
                {
                    bool method_match {false};
                    auto match = (*regex_method).second.find(0);

                    if (match != (*regex_method).second.end())
                    {
                        method_match = true;
                    }
                    else
                    {
                        match = (*regex_method).second.find(static_cast<int>(_ctx.req.method()));

                        if (match != (*regex_method).second.end())
                        {
                            method_match = true;
                        }
                    }

                    if (method_match)
                    {
                        std::regex rx_str {(*regex_method).first, rx_opts};
                        if (std::regex_match(path, rx_match, rx_str, rx_flgs))
                        {
                            // set the path
                            for (auto const& e : rx_match)
                            {
                                _ctx.req.path().emplace_back(e.str());
                            }

                            // parse target params
                            _ctx.req.params_parse();

                            // set callback function
                            auto const& user_func = match->second;

                            try
                            {
                                // run user function
                                user_func(_ctx);

                                _ctx.res.content_length(_ctx.res.body().size());
                                send(derived().shared_from_this(), std::move(_ctx.res));
                                return 0;
                            }
                            catch (int const e)
                            {
                                return e;
                            }
                            catch (unsigned int const e)
                            {
                                return static_cast<int>(e);
                            }
                            catch (Status const e)
                            {
                                return static_cast<int>(e);
                            }
                            catch (std::exception const&)
                            {
                                return 500;
                            }
                            catch (...)
                            {
                                return 500;
                            }
                        }
                    }
                }

                return 404;
            }

            void serve_error(int err)
            {
                _ctx.res.result(static_cast<unsigned int>(err));

                if (_attr->on_http_error)
                {
                    try
                    {
                        // run user function
                        _attr->on_http_error(_ctx);

                        _ctx.res.content_length(_ctx.res.body().size());
                        send(derived().shared_from_this(), std::move(_ctx.res));
                        return;
                    }
                    catch (int const e)
                    {
                        _ctx.res.result(static_cast<unsigned int>(e));
                    }
                    catch (unsigned int const e)
                    {
                        _ctx.res.result(e);
                    }
                    catch (Status const e)
                    {
                        _ctx.res.result(e);
                    }
                    catch (std::exception const&)
                    {
                        _ctx.res.result(500);
                    }
                    catch (...)
                    {
                        _ctx.res.result(500);
                    }
                }

                _ctx.res.set(Header::content_type, "text/plain");
                _ctx.res.body() = "Error: " + std::to_string(_ctx.res.result_int());
                _ctx.res.content_length(_ctx.res.body().size());
                send(derived().shared_from_this(), std::move(_ctx.res));
            };

            void handle_request()
            {
                // set default response values
                _ctx.res.version(_ctx.req.version());
                _ctx.res.keep_alive(_ctx.req.keep_alive());

                if (_ctx.req.target().empty())
                {
                    _ctx.req.target() = "/";
                }

                if (_ctx.req.target().at(0) != '/' ||
                    _ctx.req.target().find("..") != boost::beast::string_view::npos)
                {
                    this->serve_error(404);
                    return;
                }

                // serve dynamic content
                auto dyna = this->serve_dynamic();
                // success
                if (dyna == 0)
                {
                    return;
                }
                // error
                if (dyna != 404)
                {
                    this->serve_error(dyna);
                    return;
                }

                // serve static content
                auto stat = this->serve_static();
                if (stat != 0)
                {
                    this->serve_error(stat);
                    return;
                }
            }

            bool handle_websocket()
            {
                // the request path
                std::string path {_ctx.req.target().to_string()};

                // separate the query parameters
                auto params = Detail::split(path, "?", 1);
                path = params.at(0);

                // regex variables
                std::smatch rx_match {};
                std::regex_constants::syntax_option_type const rx_opts {std::regex::ECMAScript};
                std::regex_constants::match_flag_type const rx_flgs {std::regex_constants::match_not_null};

                // check for matching route
                for (auto const& [regex, callback] : _attr->websocket_routes)
                {
                    std::regex rx_str {regex, rx_opts};

                    if (std::regex_match(path, rx_match, rx_str, rx_flgs))
                    {
                        // set the path
                        for (auto const& e : rx_match)
                        {
                            _ctx.req.path().emplace_back(e.str());
                        }

                        // parse target params
                        _ctx.req.params_parse();

                        // create websocket
                        std::make_shared<Websocket_Type>
                                (derived().socket_move(), _attr, std::move(_ctx.req), callback)
                                ->run();

                        return true;
                    }
                }

                return false;
            }

            void cancel_timer()
            {
                // set the timer to expire immediately
                _timer.expires_at((std::chrono::steady_clock::time_point::min)());
            }

            void do_timer()
            {
                // wait on the timer
                _timer.async_wait(
                        net::bind_executor(_strand,
                                           [self = derived().shared_from_this()](error_code ec)
                                           {
                                               self->on_timer(ec);
                                           }
                        )
                );
            }

            void on_timer(error_code ec_ = {})
            {
                if (ec_ && ec_ != net::error::operation_aborted)
                {
                    // TODO log here
                    return;
                }

                // check if socket has been upgraded or closed
                if (_timer.expires_at() == (std::chrono::steady_clock::time_point::min)())
                {
                    return;
                }

                // check expiry
                if (_timer.expiry() <= std::chrono::steady_clock::now())
                {
                    derived().do_timeout();
                    return;
                }
            }

            void do_read()
            {
                _timer.expires_after(_attr->timeout);

                _res = nullptr;
                _ctx = {};
                _ctx.res.base() = http::response_header<>(_attr->http_headers);

                http::async_read(derived().socket(), _buf, _ctx.req,
                                 net::bind_executor(_strand,
                                                    [self = derived().shared_from_this()](error_code ec, std::size_t bytes)
                                                    {
                                                        self->on_read(ec, bytes);
                                                    }
                                 )
                );
            }

            void on_read(error_code ec_, std::size_t bytes_)
            {
                boost::ignore_unused(bytes_);

                // the timer has closed the socket
                if (ec_ == net::error::operation_aborted)
                {
                    return;
                }

                // the connection has been closed
                if (ec_ == http::error::end_of_stream)
                {
                    derived().do_shutdown();
                    return;
                }

                if (ec_)
                {
                    // TODO log here
                    return;
                }

                // check for websocket upgrade
                if (websocket::is_upgrade(_ctx.req))
                {
                    if (! _attr->websocket || _attr->websocket_routes.empty())
                    {
                        derived().do_shutdown();
                        return;
                    }

                    // upgrade to websocket
                    if (handle_websocket())
                    {
                        this->cancel_timer();
                        return;
                    }
                    else
                    {
                        derived().do_shutdown();
                        return;
                    }
                }

                if (_attr->on_http_connect)
                {
                    try
                    {
                        // run user func
                        _attr->on_http_connect(_ctx);
                    }
                    catch (...)
                    {
                    }
                }

                this->handle_request();

                if (_attr->on_http_disconnect)
                {
                    try
                    {
                        // run user func
                        _attr->on_http_disconnect(_ctx);
                    }
                    catch (...)
                    {
                    }
                }
            }

            void on_write(error_code ec_, std::size_t bytes_, bool close_)
            {
                boost::ignore_unused(bytes_);

                // the timer has closed the socket
                if (ec_ == net::error::operation_aborted)
                {
                    return;
                }

                if (ec_)
                {
                    // TODO log here
                    return;
                }

                if (close_)
                {
                    derived().do_shutdown();
                    return;
                }

                // read another request
                this->do_read();
            }

            net::strand<net::io_context::executor_type> _strand;
            net::steady_timer _timer;
            boost::beast::flat_buffer _buf;
            std::shared_ptr<Attr> const _attr;
            Http_Ctx _ctx {};
            std::shared_ptr<void> _res {nullptr};
            bool _close {false};
        }; // class Http_Base

        class Http :
                public Http_Base<Http, Websocket>,
                public std::enable_shared_from_this<Http>
        {
        public:

            Http(tcp::socket socket_, std::shared_ptr<Attr> const attr_) :
                    Http_Base<Http, Websocket> {socket_.get_executor().context(), attr_},
                    _socket {std::move(socket_)}
            {
            }

            ~Http()
            {
            }

            tcp::socket& socket()
            {
                return _socket;
            }

            tcp::socket&& socket_move()
            {
                return std::move(_socket);
            }

            void run()
            {
                this->do_timer();
                this->do_read();
            }

            void do_timeout()
            {
                this->do_shutdown();
            }

            void do_shutdown()
            {
                error_code ec;

                // send a tcp shutdown
                _socket.shutdown(tcp::socket::shutdown_send, ec);

                this->cancel_timer();

                if (ec)
                {
                    // TODO log here
                    return;
                }
            }

        private:

            tcp::socket _socket;
        }; // class Http

        template<typename Session>
        class Listener : public std::enable_shared_from_this<Listener<Session>>
        {
        public:

            Listener(net::io_context& io_, tcp::endpoint endpoint_, std::shared_ptr<Attr> const attr_) :
                    _acceptor {io_},
                    _socket {io_},
                    _attr {attr_}
            {
                error_code ec;

                // open the acceptor
                _acceptor.open(endpoint_.protocol(), ec);
                if (ec)
                {
                    // TODO log here
                    return;
                }

                // allow address reuse
                _acceptor.set_option(net::socket_base::reuse_address(true), ec);
                if (ec)
                {
                    // TODO log here
                    return;
                }

                // bind to the server address
                _acceptor.bind(endpoint_, ec);
                if (ec)
                {
                    // TODO log here
                    return;
                }

                // start listening for connections
                _acceptor.listen(net::socket_base::max_listen_connections, ec);

                if (ec)
                {
                    // TODO log here
                    return;
                }
            }

            void run()
            {
                if (! _acceptor.is_open())
                {
                    // TODO log here
                    return;
                }

                do_accept();
            }

        private:

            void do_accept()
            {
                _acceptor.async_accept(_socket,
                                       [self = this->shared_from_this()](error_code ec)
                                       {
                                           self->on_accept(ec);
                                       }
                );
            }

            void on_accept(error_code ec_)
            {
                if (ec_)
                {
                    // TODO log here
                }
                else
                {
                    // create an Http obj and run it
                    std::make_shared<Session>(std::move(_socket), _attr)->run();
                }

                // accept another connection
                do_accept();
            }

        private:

            tcp::acceptor _acceptor;
            tcp::socket _socket;
            std::shared_ptr<Attr> const _attr;
        }; // class Listener

#endif // OB_BELLE_H