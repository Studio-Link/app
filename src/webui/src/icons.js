import { library, dom } from '@fortawesome/fontawesome-svg-core'
import { faCog } from '@fortawesome/free-solid-svg-icons/faCog'
import { faRandom } from '@fortawesome/free-solid-svg-icons/faRandom'
import { faLongArrowAltRight } from '@fortawesome/free-solid-svg-icons/faLongArrowAltRight'
import { faRss } from '@fortawesome/free-solid-svg-icons/faRss'
import { faDotCircle } from '@fortawesome/free-regular-svg-icons/faDotCircle'
import { faPhone } from '@fortawesome/free-solid-svg-icons/faPhone'
import { faMicrophoneSlash } from '@fortawesome/free-solid-svg-icons/faMicrophoneSlash'
import { faPhoneSlash } from '@fortawesome/free-solid-svg-icons/faPhoneSlash'
import { faUserPlus } from '@fortawesome/free-solid-svg-icons/faUserPlus'
import { faAddressBook } from '@fortawesome/free-solid-svg-icons/faAddressBook'
import { faPhoneSquare } from '@fortawesome/free-solid-svg-icons/faPhoneSquare'
import { faPlusCircle } from '@fortawesome/free-solid-svg-icons/faPlusCircle'
import { faEdit } from '@fortawesome/free-solid-svg-icons/faEdit'
import { faTrashAlt } from '@fortawesome/free-solid-svg-icons/faTrashAlt'
import { faTh } from '@fortawesome/free-solid-svg-icons/faTh'
import { faFolderOpen } from '@fortawesome/free-regular-svg-icons/faFolderOpen'

library.add(faCog)
library.add(faRandom)
library.add(faLongArrowAltRight)
library.add(faRss)
library.add(faDotCircle)
library.add(faPhone)
library.add(faMicrophoneSlash)
library.add(faPhoneSlash)
library.add(faUserPlus)
library.add(faAddressBook)
library.add(faPhoneSquare)
library.add(faPlusCircle)
library.add(faEdit)
library.add(faTrashAlt)
library.add(faTh)
library.add(faFolderOpen)

// Replace any existing <i> tags with <svg> and set up a MutationObserver to
// continue doing this as the DOM changes.
dom.watch()
