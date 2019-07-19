import { library, dom } from '@fortawesome/fontawesome-svg-core'
import { faCog } from '@fortawesome/free-solid-svg-icons'
import { faRandom } from '@fortawesome/free-solid-svg-icons'
import { faLongArrowAltRight } from '@fortawesome/free-solid-svg-icons'
import { faRss } from '@fortawesome/free-solid-svg-icons'
import { faDotCircle } from '@fortawesome/free-regular-svg-icons'
import { faPhone } from '@fortawesome/free-solid-svg-icons'

// We are only using the user-astronaut icon
library.add(faCog)
library.add(faRandom)
library.add(faLongArrowAltRight)
library.add(faRss)
library.add(faDotCircle)
library.add(faPhone)

// Replace any existing <i> tags with <svg> and set up a MutationObserver to
// continue doing this as the DOM changes.
dom.watch()
