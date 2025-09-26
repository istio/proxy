import {useEffect, useContext} from "react"
import {Navigate} from "react-router-dom"
import {AuthContext} from "../context"
import {TAuthContext} from "../@types/app"
import Home from "./Home"
import {toaster} from "./ui/toast"

/*
Note: Envoy's oAuth implementation requires that a page be requested *after*
 a successful authorization/authentication.

 The consequence is that 2 pages are required to complete authentication -
 this one and Auth.tsx which does a hard redirect here.

*/

export default function Login() {
  const {state, dispatch} = useContext(AuthContext) as TAuthContext
  const {isLoggedIn} = state
  useEffect(() => {
    const {authenticating, failed, isLoggedIn, proxy_url} = state
    const fetchUser = async () => {
      dispatch({type: "AUTH"})
      const response = await fetch(`${proxy_url}/user`)
      const user = await response.json()
      dispatch({
        type: "LOGIN",
        payload: {user, isLoggedIn: true}
      })
    }
    if (!isLoggedIn && !authenticating && !failed) {
      fetchUser().catch(error => {
        dispatch({type: "ERROR"})
        toaster.create({
          description: `Login failed: ${error.message}`,
          type: "error",
          closable: true,
          duration: 3000,
        })
      })
    }
  }, [state, dispatch])
  if (isLoggedIn) {
    return <Navigate to="/" />
  }
  return <Home />
}
