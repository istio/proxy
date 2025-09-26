import {Tabs, Text} from '@chakra-ui/react'
import {useContext} from "react"

import {Toaster} from "./ui/toaster"
import {AuthContext} from "../context"
import {
  IHomeProps,
  TAuthContext} from "../@types/app"
import Layout from "./Layout"
import {RepoTableHeaders, RepoTr} from "./Repos"
import {RelatedUserTableHeaders, RelatedUserTr} from "./Users"
import {Resources} from "./Resources"

export const Content = () => {
  const {state: userState} = useContext(AuthContext) as TAuthContext
  const {user} = userState
  if (!userState.isLoggedIn || !user) {
    return <Text>Login to query APIs</Text>
  }
  return (
    <Tabs.Root defaultValue="repos">
      <Tabs.List>
        <Tabs.Trigger value="repos">Repos</Tabs.Trigger>
        <Tabs.Trigger value="followers">Followers</Tabs.Trigger>
        <Tabs.Trigger value="following">Following</Tabs.Trigger>
      </Tabs.List>
      <Tabs.Content value="repos">
        <Resources
          name="repos"
          title="Repositories"
          headers={RepoTableHeaders}
          row={RepoTr} />
      </Tabs.Content>
      <Tabs.Content value="followers">
        <Resources
          name="followers"
          title="Followers"
          headers={RelatedUserTableHeaders}
          row={RelatedUserTr} />
      </Tabs.Content>
      <Tabs.Content value="following">
        <Resources
          name="following"
          title="Following"
          headers={RelatedUserTableHeaders}
          row={RelatedUserTr} />
      </Tabs.Content>
    </Tabs.Root>
  )
}

export default function Home (props: IHomeProps) {
  return (
    <Layout {...props}>
      <Content />
      <Toaster />
    </Layout>
  )
}
