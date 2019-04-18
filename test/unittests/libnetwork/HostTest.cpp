/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/** @file Host.h
 * @author 122025861@qq.com
 * @date 2018
 */

#include "libnetwork/Host.h"
#include "FakeASIOInterface.h"
#include "libp2p/P2PMessageFactory.h"
#include "test/tools/libutils/Common.h"
#include <libinitializer/SecureInitializer.h>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/test/unit_test.hpp>

using namespace dev;
using namespace std;
using namespace dev::network;
using namespace dev::p2p;
using namespace dev::test;

namespace test_Host
{
struct HostFixture
{
    HostFixture()
    {
        boost::property_tree::ptree pt;
        pt.put("secure.data_path", getTestPath().string() + "/fisco-bcos-data/");
#ifdef FISCO_GM
        m_sslContext =
            std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
#else
        auto secureInitializer = std::make_shared<dev::initializer::SecureInitializer>();
        secureInitializer->initConfig(pt);
        m_sslContext = secureInitializer->SSLContext();
#endif
        m_asioInterface = std::make_shared<dev::network::FakeASIOInterface>();
        m_asioInterface->setIOService(std::make_shared<ba::io_service>());
        m_asioInterface->setSSLContext(m_sslContext);
        m_asioInterface->setType(dev::network::ASIOInterface::SSL);

        m_host = std::make_shared<dev::network::Host>();
        m_host->setASIOInterface(m_asioInterface);
        m_sessionFactory = std::make_shared<dev::network::SessionFactory>();
        m_host->setSessionFactory(m_sessionFactory);
        m_messageFactory = std::make_shared<P2PMessageFactory>();
        m_host->setMessageFactory(m_messageFactory);

        m_host->setHostPort(m_hostIP, m_port);
        m_threadPool = std::make_shared<ThreadPool>("P2PTest", 2);
        m_host->setThreadPool(m_threadPool);
        m_host->setCRL(m_certBlacklist);
        m_host->setConnectionHandler(
            [](dev::network::NetworkException e, dev::network::NodeInfo const&,
                std::shared_ptr<dev::network::SessionFace>) {
                if (e.errorCode())
                {
                    LOG(ERROR) << e.what();
                }
            });
        m_connectionHandler = m_host->connectionHandler();
    }

    ~HostFixture() {}
    string m_hostIP = "127.0.0.1";
    uint16_t m_port = 8845;
    std::shared_ptr<dev::network::Host> m_host;
    std::shared_ptr<boost::asio::ssl::context> m_sslContext;
    MessageFactory::Ptr m_messageFactory;
    std::shared_ptr<SessionFactory> m_sessionFactory;
    std::shared_ptr<dev::ThreadPool> m_threadPool;
    std::vector<std::string> m_certBlacklist;
    std::shared_ptr<ASIOInterface> m_asioInterface;
    std::function<void(NetworkException, NodeInfo const&, std::shared_ptr<SessionFace>)>
        m_connectionHandler;
};

BOOST_FIXTURE_TEST_SUITE(Host, HostFixture)

BOOST_AUTO_TEST_CASE(functions)
{
    BOOST_CHECK(m_port == m_host->listenPort());
    BOOST_CHECK(m_hostIP == m_host->listenHost());
    string hostIP = "0.0.0.0";
    uint16_t port = 8546;
    m_host->setHostPort(hostIP, port);
    BOOST_CHECK(port == m_host->listenPort());
    BOOST_CHECK(hostIP == m_host->listenHost());
    BOOST_CHECK(false == m_host->haveNetwork());
    BOOST_CHECK(m_certBlacklist == m_host->certBlacklist());
    m_host->connectionHandler();
    BOOST_CHECK(m_asioInterface == m_host->asioInterface());
    BOOST_CHECK(m_sessionFactory == m_host->sessionFactory());
    BOOST_CHECK(m_messageFactory == m_host->messageFactory());
    BOOST_CHECK(m_threadPool == m_host->threadPool());
}

BOOST_AUTO_TEST_CASE(run)
{
    m_host->start();
    // start() will create a new thread and call host->startAccept, so wait
    this_thread::sleep_for(chrono::milliseconds(200));
    BOOST_CHECK(true == m_host->haveNetwork());

    auto fakeAsioInterface = dynamic_pointer_cast<FakeASIOInterface>(m_asioInterface);
    while (!fakeAsioInterface->m_acceptorInfo.first)
    {
    }
    auto socket = fakeAsioInterface->m_acceptorInfo.first;
    auto nodeIP = NodeIPEndpoint(boost::asio::ip::address::from_string("127.0.0.1"), 0, 8888);
    socket->setNodeIPEndpoint(nodeIP);
    auto handler = fakeAsioInterface->m_acceptorInfo.second;
    boost::system::error_code ec;
    handler(ec);
    m_host->stop();
    handler = fakeAsioInterface->m_acceptorInfo.second;
    handler(ec);
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace test_Host