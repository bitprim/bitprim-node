import os
from conan.packager import ConanMultiPackager
import copy 

if __name__ == "__main__":
    builder = ConanMultiPackager(username="bitprim", channel="testing",
                                 remotes="https://api.bintray.com/conan/bitprim/bitprim",
                                 archs=["x86_64"])

    builder.add_common_builds(shared_option_name="bitprim-node:shared")

    filtered_builds = []
    for settings, options, env_vars, build_requires in builder.builds:
        if settings["build_type"] == "Release" \
                and not options["bitprim-node:shared"]:

            env_vars["BITPRIM_BUILD_NUMBER"] = os.getenv('BITPRIM_BUILD_NUMBER', '-')
                
            # filtered_builds.append([settings, options, env_vars, build_requires])

            opts_bch = copy.deepcopy(options)
            opts_btc = copy.deepcopy(options)
            # opts_ltc = copy.deepcopy(options)

            opts_bch["bitprim-node:currency"] = "BCH"
            opts_btc["bitprim-node:currency"] = "BTC"
            # opts_ltc["bitprim-node:currency"] = "LTC"

            filtered_builds.append([settings, opts_bch, env_vars, build_requires])
            filtered_builds.append([settings, opts_btc, env_vars, build_requires])
            # filtered_builds.append([settings, opts_ltc, env_vars, build_requires])

    builder.builds = filtered_builds
    builder.run()
